// Copyright 2025 robot_navigation2
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.

#ifndef robot_navigation2__WAYPOINT_RUNNER_CLIENT_HPP_
#define robot_navigation2__WAYPOINT_RUNNER_CLIENT_HPP_

#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "robot_navigation2/action/follow_waypoints_with_wait.hpp"

namespace robot_navigation2
{

/**
 * @brief 从YAML文件读取航点并执行多点导航的客户端节点
 *
 * 该节点负责:
 * 1. 从YAML配置文件读取航点列表
 * 2. 调用 FollowWaypointsWithWait action 服务
 * 3. 监控导航进度并处理反馈
 */
class WaypointRunnerClient : public rclcpp::Node
{
public:
  using FollowWaypointsWithWait = robot_navigation2::action::FollowWaypointsWithWait;
  using GoalHandleFollowWaypointsWithWait =
    rclcpp_action::ClientGoalHandle<FollowWaypointsWithWait>;

  /**
   * @brief 构造函数
   * @param options 节点选项
   */
  explicit WaypointRunnerClient(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  /**
   * @brief 从YAML文件加载航点并发送目标
   * @param yaml_file_path YAML文件路径
   * @return 是否成功启动导航任务
   */
  bool loadAndRunWaypoints(const std::string & yaml_file_path);

private:
  // Action客户端
  rclcpp_action::Client<FollowWaypointsWithWait>::SharedPtr action_client_;

  // Action服务器可用性标志
  bool action_server_ready_;
  std::mutex action_mutex_;

  // 配置参数
  std::string frame_id_;
  double action_timeout_sec_;

  // 目标句柄（用于取消）
  GoalHandleFollowWaypointsWithWait::SharedPtr current_goal_handle_;

  /**
   * @brief 从YAML文件加载航点数据
   * @param yaml_file_path YAML文件路径
   * @param poses 输出的航点位姿列表
   * @param wait_times_ms 输出的等待时间列表
   * @return 是否成功加载
   */
  bool loadWaypointsFromYaml(
    const std::string & yaml_file_path,
    std::vector<geometry_msgs::msg::PoseStamped> & poses,
    std::vector<int32_t> & wait_times_ms);

  /**
   * @brief 等待Action服务器上线
   * @param timeout_sec 超时时间（秒）
   * @return 服务器是否上线
   */
  bool waitForActionServer(double timeout_sec);

  /**
   * @brief 创建PoseStamped消息
   * @param x x坐标
   * @param y y坐标
   * @param yaw 朝向角度（弧度）
   * @return PoseStamped消息
   */
  geometry_msgs::msg::PoseStamped createPoseStamped(double x, double y, double yaw);

  /**
   * @brief Goal响应回调
   */
  void goalResponseCallback(
    const GoalHandleFollowWaypointsWithWait::SharedPtr & goal_handle);

  /**
   * @brief Feedback回调
   */
  void feedbackCallback(
    GoalHandleFollowWaypointsWithWait::SharedPtr,
    const std::shared_ptr<const FollowWaypointsWithWait::Feedback> feedback);

  /**
   * @brief Result回调
   */
  void resultCallback(
    const GoalHandleFollowWaypointsWithWait::WrappedResult & result);
};

}  // namespace robot_navigation2

#endif  // robot_navigation2__WAYPOINT_RUNNER_CLIENT_HPP_
