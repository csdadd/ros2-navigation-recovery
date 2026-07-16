#ifndef NAVIGATIONACTIONTHREAD_H
#define NAVIGATIONACTIONTHREAD_H

#include "basethread.h"
#include <rclcpp_action/rclcpp_action.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <memory>
#include <atomic>
#include <QMutex>

class NavigationActionThread : public BaseThread
{
    Q_OBJECT

public:
    explicit NavigationActionThread(QObject* parent = nullptr);
    ~NavigationActionThread();

    void sendGoalToPose(double x, double y, double yaw = 0.0);
    bool cancelCurrentGoal();
    bool isNavigating() const;
    geometry_msgs::msg::PoseStamped getCurrentGoal() const;

signals:
    void goalAccepted();
    void goalRejected(const QString& reason);
    void feedbackReceived(double distanceRemaining, double navigationTime,
                         int recoveries, double estimatedTimeRemaining);
    void resultReceived(bool success, const QString& message);
    void goalCanceled();

protected:
    void initialize() override;
    void process() override;
    void cleanup() override;
    bool usesBlockingSpin() const override { return true; }

private:
    void goalResponseCallback(
        std::shared_ptr<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>> goalHandle);
    void feedbackCallback(
        std::shared_ptr<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>> goalHandle,
        const std::shared_ptr<const nav2_msgs::action::NavigateToPose::Feedback> feedback);
    void resultCallback(
        const rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>::WrappedResult& result);

private:
    rclcpp::Node::SharedPtr m_node;
    rclcpp_action::Client<nav2_msgs::action::NavigateToPose>::SharedPtr m_actionClient;
    rclcpp::executors::SingleThreadedExecutor::SharedPtr m_executor;
    std::shared_ptr<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>> m_goalHandle;
    geometry_msgs::msg::PoseStamped m_currentGoal;
    std::atomic<bool> m_isNavigating;
    mutable QMutex m_mutex;
    std::shared_future<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>::SharedPtr> m_goalFuture;
    static constexpr int CANCEL_TIMEOUT_MS = 3000;
};

#endif // NAVIGATIONACTIONTHREAD_H
