#ifndef LOGUTILS_H
#define LOGUTILS_H

#include "loglevel.h"
#include <QString>

/**
 * @brief 日志工具类
 *
 * 提供日志相关的共享工具函数
 * 所有方法都是静态方法，无需实例化
 */
namespace LogUtils {

/**
 * @brief 将日志级别枚举转换为字符串
 * @param level 日志级别
 * @return 日志级别字符串（DEBUG/INFO/WARN/ERROR/FATAL/UNKNOWN）
 *
 * 这是统一的实现，替代各模块中的重复代码
 */
inline QString levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARN";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::FATAL:
            return "FATAL";
        case LogLevel::HIGHFREQ:
            return "HIGHFREQ";
        case LogLevel::ODOMETRY:
            return "ODOMETRY";
        case LogLevel::COLLISION:
            return "COLLISION";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief 从字符串解析日志级别
 * @param levelStr 日志级别字符串（不区分大小写）
 * @return 日志级别枚举，解析失败时返回 LogLevel::INFO
 */
inline LogLevel levelFromString(const QString& levelStr) {
    QString upper = levelStr.toUpper();
    if (upper == "DEBUG") return LogLevel::DEBUG;
    if (upper == "INFO") return LogLevel::INFO;
    if (upper == "WARN" || upper == "WARNING") return LogLevel::WARNING;
    if (upper == "ERROR") return LogLevel::ERROR;
    if (upper == "FATAL") return LogLevel::FATAL;
    if (upper == "HIGHFREQ") return LogLevel::HIGHFREQ;
    if (upper == "ODOMETRY") return LogLevel::ODOMETRY;
    if (upper == "COLLISION") return LogLevel::COLLISION;
    return LogLevel::INFO;  // 默认级别
}

} // namespace LogUtils

#endif // LOGUTILS_H
