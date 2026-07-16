#include "systemmonitorthread.h"
#include "logutils.h"
#include <QDir>
#include <QCoreApplication>
#include <thread>
#include <QDebug>

SystemMonitorThread::SystemMonitorThread(QObject* parent)
    : BaseThread(parent)
{
    m_threadName = "SystemMonitorThread";
    qDebug() << "[SystemMonitorThread] 构造函数";
}

SystemMonitorThread::~SystemMonitorThread()
{
    stopThread();
}

void SystemMonitorThread::initialize()
{
    try {
        emit logMessage("系统初始化中...", LogLevel::INFO);

        ROSContextManager::instance().initialize();

        m_rosNode = std::make_shared<rclcpp::Node>("system_monitor_node");
        m_executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();

        subscribeROSTopics();
        m_executor->add_node(m_rosNode);
        setExecutor(m_executor);

        emit logMessage("SystemMonitorThread 初始化成功", LogLevel::INFO);
        emit logMessage("系统监控已启动 - 开始收集日志和诊断信息", LogLevel::INFO);

        qDebug() << "[SystemMonitorThread] 初始化成功";
        emit connectionStateChanged(true);

    } catch (const std::exception& e) {
        emit threadError(QString("Failed to initialize SystemMonitorThread: %1").arg(e.what()));
        emit connectionStateChanged(false);
        throw;
    }
}



void SystemMonitorThread::subscribeROSTopics()
{
    m_rosoutAggSub = m_rosNode->create_subscription<rcl_interfaces::msg::Log>(
        "/rosout_agg",
        rclcpp::SensorDataQoS(),
        [this](const rcl_interfaces::msg::Log::SharedPtr msg) {
            processROSLog(msg);
        }
    );

    m_collisionSub = m_rosNode->create_subscription<sensor_msgs::msg::PointCloud2>(
        "/mobile_base/sensors/bumper_pointcloud",
        rclcpp::SensorDataQoS(),
        [this](const sensor_msgs::msg::PointCloud2::SharedPtr msg) {
            processCollisionData(msg);
        }
    );

    m_behaviorTreeSub = m_rosNode->create_subscription<nav2_msgs::msg::BehaviorTreeLog>(
        "/behavior_tree_log",
        rclcpp::SensorDataQoS(),
        [this](const nav2_msgs::msg::BehaviorTreeLog::SharedPtr msg) {
            processBehaviorTreeLog(msg);
        }
    );

    m_manualInterventionSub = m_rosNode->create_subscription<std_msgs::msg::Bool>(
        "/manual_intervention",
        rclcpp::SensorDataQoS(),
        [this](const std_msgs::msg::Bool::SharedPtr msg) {
            processManualIntervention(msg);
        }
    );
}

void SystemMonitorThread::process()
{

}

void SystemMonitorThread::cleanup()
{
    m_executor.reset();
    m_rosNode.reset();

    emit connectionStateChanged(false);
    emit logMessage("SystemMonitorThread cleanup completed", LogLevel::INFO);
}

void SystemMonitorThread::processROSLog(const rcl_interfaces::msg::Log::SharedPtr msg)
{
    QString message = QString::fromStdString(msg->msg);
    QString source = QString::fromStdString(msg->name);
    LogLevel level = LogLevel::INFO;

    if (source.contains("diagnostic") ||
        message.contains("diagnostic") ||
        message.contains("No status available")) {
        return;
    }

    switch (msg->level) {
        case rcl_interfaces::msg::Log::DEBUG:
            level = LogLevel::DEBUG;
            break;
        case rcl_interfaces::msg::Log::INFO:
            level = LogLevel::INFO;
            break;
        case rcl_interfaces::msg::Log::WARN:
            level = LogLevel::WARNING;
            break;
        case rcl_interfaces::msg::Log::ERROR:
            if (message == "No events recorded." ||
                message == "No status available") {
                level = LogLevel::DEBUG;
            } else {
                level = LogLevel::ERROR;
            }
            break;
        case rcl_interfaces::msg::Log::FATAL:
            level = LogLevel::FATAL;
            break;
        default:
            level = LogLevel::INFO;
            break;
    }

    if (message.contains("Nav2 is active")) {
        level = LogLevel::HIGHFREQ;
    }

    qint64 totalMSecs = static_cast<qint64>(msg->stamp.sec) * 1000 +
                        msg->stamp.nanosec / 1000000;
    QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(totalMSecs);

    // 为消息添加 /rosout_agg 来源标记，便于与 /diagnostics 来源区分
    QString taggedMessage = QString("%1 [/rosout_agg]").arg(message);

    emit logMessageReceived(taggedMessage, static_cast<int>(level), timestamp);

    if (level >= LogLevel::ERROR) {
        QString logMsg = QString("[%1] %2").arg(source, message);
        emit exceptionDetected(logMsg);
    }
}

void SystemMonitorThread::processCollisionData(const sensor_msgs::msg::PointCloud2::SharedPtr msg)
{
    if (msg->width * msg->height > 0) {
        QString message = QString("Collision detected! %1 points in bumper cloud").arg(msg->width * msg->height);
        QDateTime timestamp = QDateTime::currentDateTime();

        emit collisionDetected(message);
        emit logMessageReceived(message, static_cast<int>(LogLevel::COLLISION), timestamp);
    }
}

void SystemMonitorThread::processBehaviorTreeLog(const nav2_msgs::msg::BehaviorTreeLog::SharedPtr msg)
{
    for (const auto& event : msg->event_log) {
        QString message = QString::fromStdString(event.node_name);
        QString status = QString::fromStdString(event.current_status);

        if (status == "FAILURE" || status == "RUNNING") {
            QString fullMsg = QString("Behavior Tree: %1 - %2").arg(message, status);
            QDateTime timestamp = QDateTime::currentDateTime();

            LogLevel level = (status == "FAILURE") ? LogLevel::ERROR : LogLevel::WARNING;

            emit behaviorTreeLogReceived(fullMsg);
            emit logMessageReceived(fullMsg, static_cast<int>(level), timestamp);

            if (level >= LogLevel::ERROR) {
                emit exceptionDetected(fullMsg);
            }
        }
    }
}

void SystemMonitorThread::processManualIntervention(const std_msgs::msg::Bool::SharedPtr msg)
{
    emit manualInterventionReceived(msg->data);
}

void SystemMonitorThread::onDiagnosticsReceived(const QString& status, int level, const QString& message)
{
    QString finalMessage = message;

    if (message.contains("Nav2 is active")) {
        m_nav2ActiveCount++;
        if (m_nav2ActiveCount % 10 != 1) {
            return;
        }
        if (status.contains("lifecycle_manager_navigation")) {
            finalMessage = QString("[lifecycle_manager_navigation] %1").arg(message);
        } else if (status.contains("lifecycle_manager_localization")) {
            finalMessage = QString("[lifecycle_manager_localization] %1").arg(message);
        }
    }

    QDateTime timestamp = QDateTime::currentDateTime();

    LogLevel monitorLevel = LogLevel::INFO;
    // diagnostic_msgs::msg::DiagnosticStatus::OK = 0, WARN = 1, ERROR = 2, STALE = 3
    if (level == 1) {
        monitorLevel = LogLevel::WARNING;
    } else if (level >= 2) {
        monitorLevel = LogLevel::ERROR;
    }

    if (finalMessage.contains("Nav2 is active")) {
        monitorLevel = LogLevel::HIGHFREQ;
    }

    // 添加来源标记
    QString taggedMessage = QString("%1 [/rosout_agg]").arg(finalMessage);
    emit logMessageReceived(taggedMessage, static_cast<int>(monitorLevel), timestamp);

    if (level >= 2) {
        QString fullMsg = QString("[%1] %2").arg(status, finalMessage);
        emit exceptionDetected(fullMsg);
    }
}
