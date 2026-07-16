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

#ifndef CUSTOM_BT_PLUGINS__PUBLISH_INTERVENTION_STATUS_HPP_
#define CUSTOM_BT_PLUGINS__PUBLISH_INTERVENTION_STATUS_HPP_

#include <string>

#include "behaviortree_cpp_v3/action_node.h"
#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/bool.hpp"

namespace custom_bt_plugins
{

/**
 * @brief BT 节点：发布人工干预状态到 ROS2 话题
 *
 * 当导航进入人工干预阶段时，发布 true 到 /manual_intervention 话题
 * 外部程序可以订阅该话题来监听人工干预事件
 */
class PublishInterventionStatus : public BT::SyncActionNode
{
public:
  /**
   * @brief 构造函数
   * @param name 节点名称
   * @param conf BT 节点配置
   */
  PublishInterventionStatus(const std::string & name, const BT::NodeConfiguration & conf);

  PublishInterventionStatus() = delete;

  /**
   * @brief 执行节点逻辑
   * @return BT::NodeStatus 总是返回 SUCCESS
   */
  BT::NodeStatus tick() override;

  /**
   * @brief 定义节点端口
   * @return BT::PortsList 端口列表
   */
  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<bool>("status", true, "人工干预状态，true 表示需要干预")
    };
  }

private:
  rclcpp::Node::SharedPtr node_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr publisher_;
  bool published_{false};
};

}  // namespace custom_bt_plugins

#endif  // CUSTOM_BT_PLUGINS__PUBLISH_INTERVENTION_STATUS_HPP_
