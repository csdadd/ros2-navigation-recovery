#include "mapconverter.h"
#include <QtMath>
#include <cmath>
#include <QDebug>

QImage MapConverter::convertToImage(const nav_msgs::msg::OccupancyGrid::SharedPtr& map)
{
    if (!map) {
        qWarning() << "[MapConverter] 地图指针为空";
        return QImage();
    }

    int width = map->info.width;
    int height = map->info.height;

    QImage image(width, height, QImage::Format_Indexed8);

    QVector<QRgb> colorTable(256);
    for (int i = 0; i < 256; ++i) {
        if (i == 255) {
            colorTable[i] = qRgb(128, 128, 128);
        } else if (i == 0) {
            colorTable[i] = qRgb(255, 255, 255);
        } else if (i == 100) {
            colorTable[i] = qRgb(0, 0, 0);
        } else {
            int gray = 255 - (i * 255 / 100);
            colorTable[i] = qRgb(gray, gray, gray);
        }
    }
    image.setColorTable(colorTable);

    const auto& data = map->data;
    for (int i = 0; i < data.size(); ++i) {
        int value = static_cast<int>(data[i]);
        if (value == -1) {
            value = 255;
        }
        // 使用scanLine替代bits()以兼容Qt6
        image.scanLine(0)[i] = static_cast<uchar>(value);
    }

    qDebug() << "[MapConverter] 地图转换成功 - 尺寸:" << width << "x" << height << "分辨率:" << map->info.resolution;

    return image;
}

QPointF MapConverter::mapToImage(double mapX, double mapY,
                                  double resolution, double originX, double originY)
{
    double imageX = (mapX - originX) / resolution;
    double imageY = (originY + 0.0 - mapY) / resolution;
    return QPointF(imageX, imageY);
}

QPointF MapConverter::imageToMap(int imageX, int imageY,
                                  double resolution, double originX, double originY)
{
    if (resolution <= 0) {
        qWarning() << "[MapConverter] 坐标转换失败 - 分辨率无效:" << resolution;
        return QPointF(0, 0);
    }

    if (!std::isfinite(originX) || !std::isfinite(originY)) {
        qWarning() << "[MapConverter] 坐标转换失败 - 原点非有限值 originX:" << originX << "originY:" << originY;
        return QPointF(0, 0);
    }

    double mapX = originX + imageX * resolution;
    double mapY = originY - imageY * resolution;

    if (!std::isfinite(mapX) || !std::isfinite(mapY)) {
        qWarning() << "[MapConverter] 坐标转换失败 - 结果非有限值 mapX:" << mapX << "mapY:" << mapY;
        return QPointF(0, 0);
    }

    return QPointF(mapX, mapY);
}

QPolygonF MapConverter::createRobotPolygon(double x, double y, double yaw, double size)
{
    QPolygonF polygon;
    polygon << QPointF(0, -size/2)
            << QPointF(-size/2, size/2)
            << QPointF(size/2, size/2);

    QTransform transform;
    transform.translate(x, y);
    transform.rotate(qRadiansToDegrees(yaw));

    return transform.map(polygon);
}
