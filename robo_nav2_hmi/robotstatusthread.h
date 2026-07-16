#ifndef ROBOTSTATUSTHREAD_H
#define ROBOTSTATUSTHREAD_H

#include "basethread.h"
#include "roscontextmanager.h"
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float32.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <nav_msgs/msg/odometry.hpp>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <sensor_msgs/msg/time_reference.hpp>
#include <QDateTime>

class RobotStatusThread : public BaseThread
{
    Q_OBJECT

public:
    explicit RobotStatusThread(QObject* parent = nullptr);
    ~RobotStatusThread();

signals:
    void batteryStatusReceived(float voltage, float percentage);
    void positionReceived(double x, double y, double yaw);
    void odometryReceived(double x, double y, double yaw, double vx, double vy, double omega);
    void systemTimeReceived(const QString& time);
    void diagnosticsReceived(const QString& status, int level, const QString& message);
    void connectionStateChanged(bool connected);

protected:
    void initialize() override;
    void process() override;
    void cleanup() override;
    bool usesBlockingSpin() const override { return true; }

private:
    void subscribeROSTopics();
    void processBatteryData(const std_msgs::msg::Float32::SharedPtr msg);
    void processPositionData(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg);
    void processOdometryData(const nav_msgs::msg::Odometry::SharedPtr msg);
    void processDiagnosticsData(const diagnostic_msgs::msg::DiagnosticArray::SharedPtr msg);
    void processTimeData(const sensor_msgs::msg::TimeReference::SharedPtr msg);

private:
    rclcpp::Node::SharedPtr m_rosNode;
    rclcpp::executors::SingleThreadedExecutor::SharedPtr m_executor;

    rclcpp::Subscription<std_msgs::msg::Float32>::SharedPtr m_batterySub;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr m_positionSub;
    rclcpp::Subscription<nav_msgs::msg::Odometry>::SharedPtr m_odometrySub;
    rclcpp::Subscription<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr m_diagnosticsSub;
    rclcpp::Subscription<sensor_msgs::msg::TimeReference>::SharedPtr m_timeRefSub;

    float m_batteryVoltage;
    bool m_firstDiagnostics = true;
    static constexpr float BATTERY_FULL = 12.6f;
    static constexpr float BATTERY_EMPTY = 10.0f;
};

#endif // ROBOTSTATUSTHREAD_H
