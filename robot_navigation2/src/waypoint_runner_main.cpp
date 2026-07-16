// Copyright 2025 robot_navigation2
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.

#include "robot_navigation2/waypoint_runner_client.hpp"
#include "rclcpp/rclcpp.hpp"

#include <memory>
#include <string>
#include <filesystem>

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  // 构建默认yaml文件路径
  std::string default_yaml_path = std::filesystem::path(__FILE__).parent_path()
      .string() + "/../config/test_waypoints.yaml";

  auto node = std::make_shared<robot_navigation2::WaypointRunnerClient>();

  // 声明默认参数
  node->declare_parameter("waypoints_file", rclcpp::ParameterValue(default_yaml_path));

  // 检查命令行参数
  std::string yaml_file_path;
  std::vector<std::string> args = rclcpp::NodeOptions().arguments();

  for (size_t i = 1; i < static_cast<size_t>(argc); ++i) {
    std::string arg = argv[i];
    if (arg.rfind("--ros-args", 0) == 0) {
      continue;  // 跳过ROS参数
    }
    if (arg.rfind("-", 0) == 0 && arg != "-") {
      continue;  // 跳过其他选项
    }
    // 假设第一个非选项参数是YAML文件路径
    yaml_file_path = arg;
    break;
  }

  // 尝试从参数获取
  if (yaml_file_path.empty()) {
    auto param = node->get_parameter("waypoints_file");
    if (param.get_type() != rclcpp::ParameterType::PARAMETER_NOT_SET) {
      yaml_file_path = param.as_string();
    }
  }

  // 检查文件是否存在
  if (!std::filesystem::exists(yaml_file_path)) {
    RCLCPP_ERROR(
      node->get_logger(),
      "YAML file does not exist: %s\n"
      "Use: ros2 run robot_navigation2 waypoint_runner_client /path/to/waypoints.yaml",
      yaml_file_path.c_str());
    rclcpp::shutdown();
    return 1;
  }

  RCLCPP_INFO(node->get_logger(), "Using waypoints file: %s", yaml_file_path.c_str());

  // 加载并运行航点
  if (!node->loadAndRunWaypoints(yaml_file_path)) {
    RCLCPP_ERROR(node->get_logger(), "Failed to start waypoint navigation");
    rclcpp::shutdown();
    return 1;
  }

  // 旋转节点以接收回调
  rclcpp::spin(node);

  rclcpp::shutdown();
  return 0;
}
