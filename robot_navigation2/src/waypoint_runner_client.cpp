// Copyright 2025 robot_navigation2
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.

#include "robot_navigation2/waypoint_runner_client.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <iomanip>

#include "yaml-cpp/yaml.h"
#include <cmath>

namespace robot_navigation2
{

WaypointRunnerClient::WaypointRunnerClient(const rclcpp::NodeOptions & options)
: Node("waypoint_runner_client", options),
  action_server_ready_(false),
  action_timeout_sec_(600.0)
{
  RCLCPP_INFO(get_logger(), "Initializing WaypointRunnerClient node");

  // 声明参数
  declare_parameter("frame_id", rclcpp::ParameterValue("map"));
  declare_parameter("action_timeout_sec", rclcpp::ParameterValue(600.0));

  // 获取参数
  frame_id_ = get_parameter("frame_id").as_string();
  action_timeout_sec_ = get_parameter("action_timeout_sec").as_double();

  RCLCPP_INFO(
    get_logger(), "Frame ID: %s, Action timeout: %.1f sec",
    frame_id_.c_str(), action_timeout_sec_);

  // 创建Action客户端
  action_client_ = rclcpp_action::create_client<FollowWaypointsWithWait>(
    this,
    "follow_waypoints_with_wait");

  RCLCPP_INFO(get_logger(), "WaypointRunnerClient initialized");
}

bool WaypointRunnerClient::loadAndRunWaypoints(const std::string & yaml_file_path)
{
  RCLCPP_INFO(get_logger(), "Loading waypoints from: %s", yaml_file_path.c_str());

  // 检查文件是否存在
  if (!std::filesystem::exists(yaml_file_path)) {
    RCLCPP_ERROR(get_logger(), "YAML file does not exist: %s", yaml_file_path.c_str());
    return false;
  }

  // 加载航点
  std::vector<geometry_msgs::msg::PoseStamped> poses;
  std::vector<int32_t> wait_times_ms;

  if (!loadWaypointsFromYaml(yaml_file_path, poses, wait_times_ms)) {
    RCLCPP_ERROR(get_logger(), "Failed to load waypoints from YAML file");
    return false;
  }

  RCLCPP_INFO(get_logger(), "Loaded %zu waypoints", poses.size());

  // 等待Action服务器
  RCLCPP_INFO(get_logger(), "Waiting for action server...");
  if (!waitForActionServer(30.0)) {
    RCLCPP_ERROR(get_logger(), "Action server not available");
    return false;
  }

  RCLCPP_INFO(get_logger(), "Action server is ready");

  // 创建目标
  auto goal = FollowWaypointsWithWait::Goal();
  goal.poses = poses;
  goal.wait_times_ms = wait_times_ms;

  RCLCPP_INFO(
    get_logger(), "Sending goal with %zu waypoints",
    goal.poses.size());

  // 配置回调
  rclcpp_action::Client<FollowWaypointsWithWait>::SendGoalOptions send_goal_options;
  send_goal_options.goal_response_callback =
    std::bind(&WaypointRunnerClient::goalResponseCallback, this, std::placeholders::_1);
  send_goal_options.feedback_callback =
    std::bind(&WaypointRunnerClient::feedbackCallback, this, std::placeholders::_1, std::placeholders::_2);
  send_goal_options.result_callback =
    std::bind(&WaypointRunnerClient::resultCallback, this, std::placeholders::_1);

  // 发送目标
  auto goal_future = action_client_->async_send_goal(goal, send_goal_options);

  return true;
}

bool WaypointRunnerClient::loadWaypointsFromYaml(
  const std::string & yaml_file_path,
  std::vector<geometry_msgs::msg::PoseStamped> & poses,
  std::vector<int32_t> & wait_times_ms)
{
  try {
    YAML::Node yaml_config = YAML::LoadFile(yaml_file_path);

    // 读取全局配置（可选）
    if (yaml_config["global_config"]) {
      auto global_config = yaml_config["global_config"];
      if (global_config["frame_id"]) {
        frame_id_ = global_config["frame_id"].as<std::string>();
        RCLCPP_INFO(get_logger(), "Using frame_id from config: %s", frame_id_.c_str());
      }
      if (global_config["action_timeout_sec"]) {
        action_timeout_sec_ = global_config["action_timeout_sec"].as<double>();
        RCLCPP_INFO(
          get_logger(), "Using action_timeout_sec from config: %.1f",
          action_timeout_sec_);
      }
    }

    // 读取航点
    if (!yaml_config["waypoints"]) {
      RCLCPP_ERROR(get_logger(), "No 'waypoints' key found in YAML file");
      return false;
    }

    auto waypoints_node = yaml_config["waypoints"];
    if (!waypoints_node.IsSequence()) {
      RCLCPP_ERROR(get_logger(), "'waypoints' should be a sequence");
      return false;
    }

    poses.clear();
    wait_times_ms.clear();

    for (size_t i = 0; i < waypoints_node.size(); ++i) {
      auto waypoint = waypoints_node[i];
      if (!waypoint.IsSequence() || waypoint.size() != 4) {
        RCLCPP_ERROR(
          get_logger(),
          "Waypoint %zu should be a sequence of 4 elements [x, y, yaw, wait_ms]", i);
        return false;
      }

      double x = waypoint[0].as<double>();
      double y = waypoint[1].as<double>();
      double yaw = waypoint[2].as<double>();
      int32_t wait_ms = waypoint[3].as<int32_t>();

      RCLCPP_INFO(
        get_logger(),
        "Waypoint %zu: x=%.3f, y=%.3f, yaw=%.3f, wait=%dms",
        i, x, y, yaw, wait_ms);

      poses.push_back(createPoseStamped(x, y, yaw));
      wait_times_ms.push_back(wait_ms);
    }

    return !poses.empty();

  } catch (const YAML::Exception & e) {
    RCLCPP_ERROR(get_logger(), "YAML parsing error: %s", e.what());
    return false;
  } catch (const std::exception & e) {
    RCLCPP_ERROR(get_logger(), "Error loading YAML: %s", e.what());
    return false;
  }
}

bool WaypointRunnerClient::waitForActionServer(double timeout_sec)
{
  std::lock_guard<std::mutex> lock(action_mutex_);
  return action_client_->wait_for_action_server(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::duration<double>(timeout_sec)));
}

geometry_msgs::msg::PoseStamped WaypointRunnerClient::createPoseStamped(
  double x, double y, double yaw)
{
  geometry_msgs::msg::PoseStamped pose;
  pose.header.frame_id = frame_id_;
  pose.header.stamp = now();

  pose.pose.position.x = x;
  pose.pose.position.y = y;
  pose.pose.position.z = 0.0;

  // 将yaw转换为四元数 (绕z轴旋转)
  // q = [cos(yaw/2), 0, 0, sin(yaw/2)]
  pose.pose.orientation.x = 0.0;
  pose.pose.orientation.y = 0.0;
  pose.pose.orientation.z = std::sin(yaw / 2.0);
  pose.pose.orientation.w = std::cos(yaw / 2.0);

  return pose;
}

void WaypointRunnerClient::goalResponseCallback(
  const GoalHandleFollowWaypointsWithWait::SharedPtr & goal_handle)
{
  std::lock_guard<std::mutex> lock(action_mutex_);

  if (!goal_handle) {
    RCLCPP_ERROR(get_logger(), "Goal was rejected by server");
  } else {
    current_goal_handle_ = goal_handle;
    RCLCPP_INFO(get_logger(), "Goal accepted by server, waiting for result");
  }
}

void WaypointRunnerClient::feedbackCallback(
  GoalHandleFollowWaypointsWithWait::SharedPtr,
  const std::shared_ptr<const FollowWaypointsWithWait::Feedback> feedback)
{
  RCLCPP_INFO(
    get_logger(), "Current waypoint: %u", feedback->current_waypoint);
}

void WaypointRunnerClient::resultCallback(
  const GoalHandleFollowWaypointsWithWait::WrappedResult & result)
{
  switch (result.code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
      RCLCPP_INFO(get_logger(), "Goal succeeded!");
      if (!result.result->missed_waypoints.empty()) {
        RCLCPP_WARN(
          get_logger(), "Missed waypoints: %zu",
          result.result->missed_waypoints.size());
      }
      break;
    case rclcpp_action::ResultCode::ABORTED:
      RCLCPP_ERROR(get_logger(), "Goal was aborted");
      break;
    case rclcpp_action::ResultCode::CANCELED:
      RCLCPP_WARN(get_logger(), "Goal was canceled");
      break;
    default:
      RCLCPP_ERROR(get_logger(), "Unknown result code");
      break;
  }

  std::lock_guard<std::mutex> lock(action_mutex_);
  current_goal_handle_.reset();

  // 任务完成后退出节点
  rclcpp::shutdown();
}

}  // namespace robot_navigation2
