#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <builtin_interfaces/msg/time.hpp>
#include <QDateTime>
#include <QString>

/**
 * @brief 时间工具类
 *
 * 提供ROS时间和Qt时间之间的转换功能
 * 所有方法都是静态方法，无需实例化
 */
namespace TimeUtils {

/**
 * @brief 将ROS时间戳转换为毫秒数
 * @param stamp ROS时间戳
 * @return 自1970-01-01 00:00:00 UTC以来的毫秒数
 *
 * 注意：ROS时间戳的sec字段是秒，nanosec字段是纳秒
 */
inline qint64 rosTimeToMSecs(const builtin_interfaces::msg::Time& stamp) {
    return static_cast<qint64>(stamp.sec) * 1000 + stamp.nanosec / 1000000;
}

/**
 * @brief 将ROS时间戳转换为QDateTime
 * @param stamp ROS时间戳
 * @return QDateTime对象
 */
inline QDateTime rosTimeToDateTime(const builtin_interfaces::msg::Time& stamp) {
    return QDateTime::fromMSecsSinceEpoch(rosTimeToMSecs(stamp));
}

/**
 * @brief 将QDateTime转换为ISO 8601格式的字符串
 * @param dt QDateTime对象
 * @return ISO 8601格式字符串，例如 "2025-01-29T15:30:45"
 */
inline QString formatTimestamp(const QDateTime& dt) {
    return dt.toString(Qt::ISODate);
}

/**
 * @brief 将ROS时间戳格式化为ISO 8601字符串
 * @param stamp ROS时间戳
 * @return ISO 8601格式字符串
 */
inline QString formatRosTimestamp(const builtin_interfaces::msg::Time& stamp) {
    return formatTimestamp(rosTimeToDateTime(stamp));
}

} // namespace TimeUtils

#endif // TIMEUTILS_H
