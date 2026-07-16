// Copyright (c) 2024 Robot Navigation2
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "wheeltec_nav2/publish_intervention_status.hpp"

namespace wheeltec_nav2
{

PublishInterventionStatus::PublishInterventionStatus(
  const std::string & name,
  const BT::NodeConfiguration & conf)
: BT::SyncActionNode(name, conf)
{
  // 从黑板获取 ROS2 节点
  node_ = config().blackboard->get<rclcpp::Node::SharedPtr>("node");

  // 创建发布器
  publisher_ = node_->create_publisher<std_msgs::msg::Bool>(
    "/manual_intervention",
    rclcpp::QoS(10).transient_local());

  RCLCPP_INFO(
    node_->get_logger(),
    "PublishInterventionStatus 节点已初始化，话题: /manual_intervention");
}

BT::NodeStatus PublishInterventionStatus::tick()
{
  bool status = true;
  getInput("status", status);

  auto msg = std_msgs::msg::Bool();
  msg.data = status;

  publisher_->publish(msg);

  RCLCPP_WARN(
    node_->get_logger(),
    "人工干预状态已发布: %s", status ? "true" : "false");

  return BT::NodeStatus::SUCCESS;
}

}  // namespace wheeltec_nav2

#include "behaviortree_cpp_v3/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<wheeltec_nav2::PublishInterventionStatus>("PublishInterventionStatus");
}
