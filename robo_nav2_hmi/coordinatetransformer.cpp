#include "coordinatetransformer.h"
#include <QPointF>
#include <QDebug>
#include <QMetaType>

Q_DECLARE_METATYPE(CoordinateTransformer)

CoordinateTransformer::CoordinateTransformer()
    : CoordinateTransformer(0.05, 0.0, 0.0, 0) {}

CoordinateTransformer::CoordinateTransformer(double resolution, double originX, double originY, int imageHeight)
    : resolution_(resolution), originX_(originX), originY_(originY), imageHeight_(imageHeight) {
    if (resolution <= 0) {
        qWarning() << "[CoordinateTransformer] 警告：分辨率应为正数，当前值:" << resolution;
    }
}

QPointF CoordinateTransformer::mapToQt(double mapX, double mapY) const {
    // 输入验证：检查坐标是否在合理范围
    if (!std::isfinite(mapX) || !std::isfinite(mapY)) {
        qWarning() << "[CoordinateTransformer] mapToQt: 输入坐标非有限值";
        return QPointF(0, 0);
    }

    if (std::abs(mapX) > MAX_MAP_COORD || std::abs(mapY) > MAX_MAP_COORD) {
        qWarning() << "[CoordinateTransformer] mapToQt: 坐标超出合理范围"
                   << "mapX:" << mapX << "mapY:" << mapY;
    }

    // 使用Nav2ViewWidget的公式（保留现有行为）
    double qtX = (mapX - originX_) / resolution_;
    double qtY = imageHeight_ - (mapY - originY_) / resolution_;

    return QPointF(qtX, qtY);
}

void CoordinateTransformer::qtToMap(const QPointF& qt, double& mapX, double& mapY) const {
    // 输入验证：检查Qt坐标是否在图像尺寸范围内
    if (imageWidth_ > 0 && imageHeight_ > 0) {
        if (qt.x() < 0 || qt.x() > imageWidth_ || qt.y() < 0 || qt.y() > imageHeight_) {
            qWarning() << "[CoordinateTransformer] qtToMap: Qt坐标超出图像范围"
                       << "qt:" << qt << "imageSize:" << imageWidth_ << "x" << imageHeight_;
        }
    }

    if (!std::isfinite(qt.x()) || !std::isfinite(qt.y())) {
        qWarning() << "[CoordinateTransformer] qtToMap: Qt坐标非有限值";
        mapX = 0.0;
        mapY = 0.0;
        return;
    }

    // 使用Nav2ViewWidget的公式（保留现有行为）
    mapX = originX_ + qt.x() * resolution_;
    mapY = originY_ + (imageHeight_ - qt.y()) * resolution_;

    if (!std::isfinite(mapX) || !std::isfinite(mapY)) {
        qWarning() << "[CoordinateTransformer] qtToMap: 转换结果非有限值"
                   << "mapX:" << mapX << "mapY:" << mapY;
        mapX = 0.0;
        mapY = 0.0;
    }
}

void CoordinateTransformer::setImageSize(int width, int height) {
    imageWidth_ = width;
    imageHeight_ = height;
}
