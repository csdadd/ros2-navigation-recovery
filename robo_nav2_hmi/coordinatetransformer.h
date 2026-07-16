#ifndef COORDINATETRANSFORMER_H
#define COORDINATETRANSFORMER_H

#include <cmath>

// 前置声明，避免包含 Qt 头文件导致的 #pragma 位置问题
class QPointF;

/**
 * @brief 坐标转换工具类
 *
 * 提供地图坐标系和Qt图像坐标系之间的转换
 *
 * 坐标系说明：
 * - 地图坐标系：原点在地图左下角，X轴向右，Y轴向上，单位为米
 * - Qt图像坐标系：原点在图像左上角，X轴向右，Y轴向下，单位为像素
 *
 * 转换公式（与Nav2ViewWidget保持一致）：
 * - qtX = (mapX - originX) / resolution
 * - qtY = imageHeight - (mapY - originY) / resolution
 * - mapX = originX + qtX * resolution
 * - mapY = originY + (imageHeight - qtY) * resolution
 */
class CoordinateTransformer {
public:
    /**
     * @brief 默认构造函数（用于 Q_DECLARE_METATYPE）
     */
    CoordinateTransformer();

    /**
     * @brief 构造函数
     * @param resolution 地图分辨率（米/像素）
     * @param originX 地图原点X坐标（米）
     * @param originY 地图原点Y坐标（米）
     * @param imageHeight 图像高度（像素），用于Y轴转换
     */
    CoordinateTransformer(double resolution, double originX, double originY, int imageHeight);

    /**
     * @brief 将地图坐标转换为Qt坐标
     * @param mapX 地图X坐标（米）
     * @param mapY 地图Y坐标（米）
     * @return Qt坐标点（像素）
     *
     * 输入验证：检查坐标是否在合理范围（±1000米）
     */
    QPointF mapToQt(double mapX, double mapY) const;

    /**
     * @brief 将Qt坐标转换为地图坐标
     * @param qt Qt坐标点（像素）
     * @param mapX 输出参数：地图X坐标（米）
     * @param mapY 输出参数：地图Y坐标（米）
     *
     * 输入验证：检查Qt坐标是否在图像尺寸范围内
     */
    void qtToMap(const QPointF& qt, double& mapX, double& mapY) const;

    /**
     * @brief 设置地图尺寸（用于输入验证）
     * @param width 图像宽度（像素）
     * @param height 图像高度（像素）
     */
    void setImageSize(int width, int height);

    // Getter方法
    double resolution() const { return resolution_; }
    double originX() const { return originX_; }
    double originY() const { return originY_; }
    int imageHeight() const { return imageHeight_; }

private:
    double resolution_;    // 地图分辨率（米/像素）
    double originX_;       // 地图原点X坐标（米）
    double originY_;       // 地图原点Y坐标（米）
    int imageWidth_ = 0;   // 图像宽度（像素），用于验证
    int imageHeight_;      // 图像高度（像素），用于Y轴转换和验证

    // 常量：坐标范围验证
    static constexpr double MAX_MAP_COORD = 1000.0;
};

#endif // COORDINATETRANSFORMER_H
