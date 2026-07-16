#ifndef GEOMETRYUTILS_H
#define GEOMETRYUTILS_H

#include <geometry_msgs/msg/quaternion.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/utils.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <algorithm>

/**
 * @brief 视图变换结构
 *
 * 存储地图到控件的缩放和偏移信息
 */
struct ViewTransform {
    double scale;
    double offset_x;
    double offset_y;
};

/**
 * @brief 计算视图变换参数
 *
 * 计算地图在控件中等比例缩放后的缩放比例和居中偏移
 *
 * @param widget_width 控件宽度
 * @param widget_height 控件高度
 * @param map_width 地图宽度
 * @param map_height 地图高度
 * @return ViewTransform 包含缩放比例和偏移量的结构
 */
inline ViewTransform calculateViewTransform(double widget_width, double widget_height,
                                             double map_width, double map_height) {
    ViewTransform vt{1.0, 0.0, 0.0};
    if (map_width > 0 && map_height > 0) {
        double scale_x = widget_width / map_width;
        double scale_y = widget_height / map_height;
        vt.scale = std::min(scale_x, scale_y);
        double scaled_width = map_width * vt.scale;
        double scaled_height = map_height * vt.scale;
        vt.offset_x = (widget_width - scaled_width) / 2.0;
        vt.offset_y = (widget_height - scaled_height) / 2.0;
    }
    return vt;
}

/**
 * @brief 几何工具类
 *
 * 提供基于tf2库的四元数和欧拉角转换功能
 * 所有方法都是静态方法，无需实例化
 */
namespace GeometryUtils {

/**
 * @brief 从四元数提取yaw角（绕Z轴旋转）
 * @param q 几何消息四元数
 * @return yaw角（弧度），范围 [-π, π]
 *
 * 使用tf2库的标准实现，避免手动计算错误
 */
inline double quaternionToYaw(const geometry_msgs::msg::Quaternion& q) {
    tf2::Quaternion tf_q(q.x, q.y, q.z, q.w);
    return tf2::getYaw(tf_q);
}

/**
 * @brief 从欧拉角创建四元数
 * @param roll  绕X轴旋转（弧度）
 * @param pitch 绕Y轴旋转（弧度）
 * @param yaw   绕Z轴旋转（弧度）
 * @return 几何消息四元数
 *
 * 使用tf2库的标准实现
 */
inline geometry_msgs::msg::Quaternion eulerToQuaternion(double roll, double pitch, double yaw) {
    tf2::Quaternion q;
    q.setRPY(roll, pitch, yaw);
    return tf2::toMsg(q);
}

/**
 * @brief 从yaw角创建四元数（简化版，只处理绕Z轴旋转）
 * @param yaw 绕Z轴旋转（弧度）
 * @return 几何消息四元数
 *
 * 适用于只需要2D旋转的场景
 */
inline geometry_msgs::msg::Quaternion yawToQuaternion(double yaw) {
    return eulerToQuaternion(0.0, 0.0, yaw);
}

} // namespace GeometryUtils

#endif // GEOMETRYUTILS_H
