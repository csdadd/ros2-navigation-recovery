#ifndef SYSTEMMONITORTHREAD_H
#define SYSTEMMONITORTHREAD_H

#include "basethread.h"
#include "roscontextmanager.h"
#include "loglevel.h"
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <rcl_interfaces/msg/log.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <nav2_msgs/msg/behavior_tree_log.hpp>
#include <std_msgs/msg/bool.hpp>
#include <QDateTime>

class SystemMonitorThread : public BaseThread
{
    Q_OBJECT

public:
    explicit SystemMonitorThread(QObject* parent = nullptr);
    ~SystemMonitorThread();

public slots:
    void onDiagnosticsReceived(const QString& status, int level, const QString& message);

signals:
    void logMessageReceived(const QString& message, int level, const QDateTime& timestamp);
    void collisionDetected(const QString& message);
    void exceptionDetected(const QString& message);
    void behaviorTreeLogReceived(const QString& log);
    void connectionStateChanged(bool connected);
    void manualInterventionReceived(bool needsIntervention);

protected:
    void initialize() override;
    void process() override;
    void cleanup() override;

private:
    void subscribeROSTopics();
    void processROSLog(const rcl_interfaces::msg::Log::SharedPtr msg);
    void processCollisionData(const sensor_msgs::msg::PointCloud2::SharedPtr msg);//存疑
    void processBehaviorTreeLog(const nav2_msgs::msg::BehaviorTreeLog::SharedPtr msg);
    void processManualIntervention(const std_msgs::msg::Bool::SharedPtr msg);

private:
    rclcpp::Node::SharedPtr m_rosNode;
    rclcpp::executors::SingleThreadedExecutor::SharedPtr m_executor;

    rclcpp::Subscription<rcl_interfaces::msg::Log>::SharedPtr m_rosoutAggSub;
    rclcpp::Subscription<sensor_msgs::msg::PointCloud2>::SharedPtr m_collisionSub;
    rclcpp::Subscription<nav2_msgs::msg::BehaviorTreeLog>::SharedPtr m_behaviorTreeSub;
    rclcpp::Subscription<std_msgs::msg::Bool>::SharedPtr m_manualInterventionSub;

    int m_nav2ActiveCount = 0;  // Nav2 is active 日志采样计数器
};

#endif // SYSTEMMONITORTHREAD_H
