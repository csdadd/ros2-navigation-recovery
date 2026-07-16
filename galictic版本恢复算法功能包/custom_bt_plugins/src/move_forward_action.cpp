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

#include "custom_bt_plugins/move_forward_action.hpp"

namespace custom_bt_plugins
{

MoveForwardAction::MoveForwardAction(
  const std::string & xml_tag_name,
  const std::string & action_name,
  const BT::NodeConfiguration & conf)
: nav2_behavior_tree::BtActionNode<nav2_msgs::action::MoveForward>(xml_tag_name, action_name, conf)
{
  double dist;
  getInput("forward_dist", dist);
  double speed;
  getInput("speed", speed);

  // Populate the input message
  goal_.target.x = dist;
  goal_.target.y = 0.0;
  goal_.target.z = 0.0;
  goal_.speed = speed;
}

void MoveForwardAction::on_tick()
{
  increment_recovery_count();
}

}  // namespace custom_bt_plugins

#include "behaviortree_cpp_v3/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  BT::NodeBuilder builder =
    [](const std::string & name, const BT::NodeConfiguration & config)
    {
      return std::make_unique<custom_bt_plugins::MoveForwardAction>(
        name, "move_forward", config);
    };

  factory.registerBuilder<custom_bt_plugins::MoveForwardAction>("MoveForward", builder);
}
