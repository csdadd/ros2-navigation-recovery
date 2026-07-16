#include "navigationactionthread.h"
#include "roscontextmanager.h"
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include <QDebug>

NavigationActionThread::NavigationActionThread(QObject* parent)
    : BaseThread(parent)
    , m_isNavigating(false)
{
    m_threadName = "NavigationActionThread";
    qDebug() << "[NavigationActionThread] 构造函数";
}

NavigationActionThread::~NavigationActionThread()
{
    qDebug() << "[NavigationActionThread] 析构函数";

    // 确保线程已停止
    stopThread();

    // 等待线程完全停止
    if (isRunning()) {
        bool stopped = wait(5000);
        if (!stopped) {
            qWarning() << "[NavigationActionThread] 析构时等待线程停止超时，强制终止";
            terminate();
            wait();
        }
    }

    qDebug() << "[NavigationActionThread] 析构完成";
}

void NavigationActionThread::initialize()
{
    qDebug() << "[NavigationActionThread] initialize - 开始初始化";

    ROSContextManager::instance().initialize();

    // 创建 Node
    m_node = std::make_shared<rclcpp::Node>("navigation_action_thread_node");

    // 创建 ActionClient
    m_actionClient = rclcpp_action::create_client<nav2_msgs::action::NavigateToPose>(
        m_node,
        "/navigate_to_pose"
    );

    // 创建 SingleThreadedExecutor 并添加 node
    m_executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    m_executor->add_node(m_node);

    qDebug() << "[NavigationActionThread] initialize - 初始化完成";
}

void NavigationActionThread::process()
{
    // 使用 spin_once() 实现可中断的阻塞式 spin
    qDebug() << "[NavigationActionThread] process - 开始可中断的 spin 循环";
    while (rclcpp::ok() && !m_stopped) {
        if (m_executor) {
            m_executor->spin_once(std::chrono::milliseconds(100));
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    qDebug() << "[NavigationActionThread] process - spin 循环已返回";
}

void NavigationActionThread::cleanup()
{
    qDebug() << "[NavigationActionThread] cleanup - 开始清理";

    // 先取消导航目标（在持有锁的情况下）
    {
        QMutexLocker locker(&m_mutex);

        if (m_isNavigating.load() && m_goalHandle && m_actionClient) {
            qWarning() << "[NavigationActionThread] cleanup 中取消导航目标";
            auto cancel_future = m_actionClient->async_cancel_goal(m_goalHandle);
            locker.unlock();  // 在等待前解锁
            cancel_future.wait_for(std::chrono::milliseconds(CANCEL_TIMEOUT_MS));
        }
    }

    // 取消 executor，让 spin() 返回
    if (m_executor) {
        m_executor->cancel();
        // 等待 spin() 真正返回
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    qDebug() << "[NavigationActionThread] cleanup - 清理完成";
}

void NavigationActionThread::sendGoalToPose(double x, double y, double yaw)
{
    qInfo() << "[NavigationActionThread] 发送导航目标:" << x << "," << y << "," << yaw;

    if (!m_actionClient->wait_for_action_server(std::chrono::seconds(5))) {
        qCritical() << "[NavigationActionThread] 导航服务器不可用";
        emit goalRejected("Action server not available");
        return;
    }

    auto goal_msg = nav2_msgs::action::NavigateToPose::Goal();
    goal_msg.pose.header.frame_id = "map";
    goal_msg.pose.header.stamp = m_node->now();
    goal_msg.pose.pose.position.x = x;
    goal_msg.pose.pose.position.y = y;
    goal_msg.pose.pose.position.z = 0.0;

    tf2::Quaternion q;
    q.setRPY(0, 0, yaw);
    goal_msg.pose.pose.orientation = tf2::toMsg(q);

    QMutexLocker locker(&m_mutex);
    m_currentGoal = goal_msg.pose;
    locker.unlock();

    qDebug() << "[NavigationActionThread] 发送导航目标 - X:" << x << ", Y:" << y << ", Yaw:" << yaw;

    auto goal_options = rclcpp_action::Client<nav2_msgs::action::NavigateToPose>::SendGoalOptions();

    // 使用 lambda 捕获 this 设置回调
    goal_options.goal_response_callback =
        [this](std::shared_ptr<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>> goalHandle) {
            goalResponseCallback(goalHandle);
        };

    goal_options.feedback_callback =
        [this](std::shared_ptr<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>> goalHandle,
               const std::shared_ptr<const nav2_msgs::action::NavigateToPose::Feedback> feedback) {
            feedbackCallback(goalHandle, feedback);
        };

    goal_options.result_callback =
        [this](const rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>::WrappedResult& result) {
            resultCallback(result);
        };

    auto goal_future = m_actionClient->async_send_goal(goal_msg, goal_options);
    m_goalFuture = goal_future;
}

bool NavigationActionThread::cancelCurrentGoal()
{
    QMutexLocker locker(&m_mutex);

    if (!m_goalHandle) {
        qWarning() << "[NavigationActionThread] 尝试取消目标，但没有可用的目标句柄";
        return false;
    }

    qInfo() << "[NavigationActionThread] 正在取消导航目标...";
    auto cancel_future = m_actionClient->async_cancel_goal(m_goalHandle);
    auto status = cancel_future.wait_for(std::chrono::milliseconds(CANCEL_TIMEOUT_MS));

    if (status == std::future_status::timeout) {
        qWarning() << "[NavigationActionThread] 取消导航目标超时";
        return false;
    }

    return true;
}

bool NavigationActionThread::isNavigating() const
{
    return m_isNavigating.load();
}

geometry_msgs::msg::PoseStamped NavigationActionThread::getCurrentGoal() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentGoal;
}

void NavigationActionThread::goalResponseCallback(
    std::shared_ptr<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>> goalHandle)
{
    QMutexLocker locker(&m_mutex);

    if (!goalHandle) {
        m_isNavigating.store(false);
        qCritical() << "[NavigationActionThread] 导航目标被服务器拒绝";
        emit goalRejected("Goal was rejected by server");
        return;
    }

    qInfo() << "[NavigationActionThread] 目标已接受";
    m_goalHandle = goalHandle;
    m_isNavigating.store(true);
    emit goalAccepted();
}

void NavigationActionThread::feedbackCallback(
    std::shared_ptr<rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>> goalHandle,
    const std::shared_ptr<const nav2_msgs::action::NavigateToPose::Feedback> feedback)
{
    double distanceRemaining = feedback->distance_remaining;
    double navigationTime = feedback->navigation_time.sec + feedback->navigation_time.nanosec / 1e9;
    int recoveries = feedback->number_of_recoveries;
    double estimatedTimeRemaining = feedback->estimated_time_remaining.sec + feedback->estimated_time_remaining.nanosec / 1e9;

    // 每50次输出一条调试日志
    static int feedbackCount = 0;
    feedbackCount++;
    if (feedbackCount % 50 == 1) {
        qDebug() << "[NavigationActionThread] Feedback #" << feedbackCount
                 << "剩余距离:" << distanceRemaining
                 << "导航时间:" << navigationTime
                 << "恢复次数:" << recoveries;
    }

    emit feedbackReceived(distanceRemaining, navigationTime, recoveries, estimatedTimeRemaining);
}

void NavigationActionThread::resultCallback(
    const rclcpp_action::ClientGoalHandle<nav2_msgs::action::NavigateToPose>::WrappedResult& result)
{
    QMutexLocker locker(&m_mutex);

    m_isNavigating.store(false);
    m_goalHandle.reset();

    switch (result.code) {
        case rclcpp_action::ResultCode::SUCCEEDED:
            qDebug() << "[NavigationActionThread] 导航成功完成";
            emit resultReceived(true, "Navigation succeeded");
            break;
        case rclcpp_action::ResultCode::ABORTED:
            qCritical() << "[NavigationActionThread] 导航被中止";
            emit resultReceived(false, "Navigation was aborted");
            break;
        case rclcpp_action::ResultCode::CANCELED:
            qDebug() << "[NavigationActionThread] 导航已取消";
            emit goalCanceled();
            break;
        default:
            qCritical() << "[NavigationActionThread] 未知导航结果代码";
            emit resultReceived(false, "Unknown result code");
            break;
    }
}
