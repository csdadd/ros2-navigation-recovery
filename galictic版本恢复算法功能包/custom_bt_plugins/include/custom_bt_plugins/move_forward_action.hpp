// Copyright (c) 2024
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

#ifndef CUSTOM_BT_PLUGINS__MOVE_FORWARD_ACTION_HPP_
#define CUSTOM_BT_PLUGINS__MOVE_FORWARD_ACTION_HPP_

#include <string>
#include <memory>

#include "nav2_behavior_tree/bt_action_node.hpp"
#include "nav2_msgs/action/move_forward.hpp"

namespace custom_bt_plugins
{

/**
 * @brief A custom_bt_plugins::BtActionNode class that wraps nav2_msgs::action::MoveForward
 */
class MoveForwardAction : public nav2_behavior_tree::BtActionNode<nav2_msgs::action::MoveForward>
{
public:
  /**
   * @brief A constructor for custom_bt_plugins::MoveForwardAction
   * @param xml_tag_name Name for the XML tag for this node
   * @param action_name Action name this node creates a client for
   * @param conf BT node configuration
   */
  MoveForwardAction(
    const std::string & xml_tag_name,
    const std::string & action_name,
    const BT::NodeConfiguration & conf);

  /**
   * @brief Function to perform some user-defined operation on tick
   */
  void on_tick() override;

  /**
   * @brief Creates list of BT ports
   * @return BT::PortsList Containing basic ports along with node-specific ports
   */
  static BT::PortsList providedPorts()
  {
    return providedBasicPorts(
      {
        BT::InputPort<double>("forward_dist", 0.15, "Distance to move forward"),
        BT::InputPort<double>("speed", 0.025, "Speed at which to move forward")
      });
  }
};

}  // namespace custom_bt_plugins

#endif  // CUSTOM_BT_PLUGINS__MOVE_FORWARD_ACTION_HPP_
