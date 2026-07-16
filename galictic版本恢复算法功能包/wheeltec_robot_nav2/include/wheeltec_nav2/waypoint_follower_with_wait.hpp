// Copyright (c) 2019 Samsung Research America
// Modified for per-waypoint wait times
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

#ifndef wheeltec_nav2__WAYPOINT_FOLLOWER_WITH_WAIT_HPP_
#define wheeltec_nav2__WAYPOINT_FOLLOWER_WITH_WAIT_HPP_

#include <memory>
#include <string>
#include <vector>

#include "nav2_util/lifecycle_node.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "wheeltec_nav2/action/follow_waypoints_with_wait.hpp"
#include "nav2_util/simple_action_server.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

#include "nav2_util/node_utils.hpp"
#include "nav2_core/waypoint_task_executor.hpp"
#include "pluginlib/class_loader.hpp"

namespace wheeltec_nav2
{

enum class ActionStatus
{
  UNKNOWN = 0,
  PROCESSING = 1,
  FAILED = 2,
  SUCCEEDED = 3
};

// 执行器状态枚举
enum class ExecutorState
{
  IDLE = 0,
  NAVIGATING = 1,
  WAITING_AT_WAYPOINT = 2
};

class WaypointFollowerWithWait : public nav2_util::LifecycleNode
{
public:
  using ActionT = wheeltec_nav2::action::FollowWaypointsWithWait;
  using ClientT = nav2_msgs::action::NavigateToPose;
  using ActionServer = nav2_util::SimpleActionServer<ActionT>;

  explicit WaypointFollowerWithWait(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());
  ~WaypointFollowerWithWait();

protected:
  nav2_util::CallbackReturn on_configure(const rclcpp_lifecycle::State & state) override;
  nav2_util::CallbackReturn on_activate(const rclcpp_lifecycle::State & state) override;
  nav2_util::CallbackReturn on_deactivate(const rclcpp_lifecycle::State & state) override;
  nav2_util::CallbackReturn on_cleanup(const rclcpp_lifecycle::State & state) override;
  nav2_util::CallbackReturn on_shutdown(const rclcpp_lifecycle::State & state) override;

  void followWaypoints();

  void resultCallback(const rclcpp_action::ClientGoalHandle<ClientT>::WrappedResult & result);
  void goalResponseCallback(const rclcpp_action::ClientGoalHandle<ClientT>::SharedPtr & goal);

  rcl_interfaces::msg::SetParametersResult
  dynamicParametersCallback(std::vector<rclcpp::Parameter> parameters);

  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr dyn_params_handler_;

  std::unique_ptr<ActionServer> action_server_;
  rclcpp_action::Client<ClientT>::SharedPtr nav_to_pose_client_;
  rclcpp::CallbackGroup::SharedPtr callback_group_;
  rclcpp::executors::SingleThreadedExecutor callback_group_executor_;
  std::shared_future<rclcpp_action::ClientGoalHandle<ClientT>::SharedPtr> future_goal_handle_;
  bool stop_on_failure_;
  ActionStatus current_goal_status_;
  int loop_rate_;
  std::vector<int> failed_ids_;
  std::vector<int32_t> wait_times_ms_;

  // 执行器状态
  ExecutorState executor_state_{ExecutorState::IDLE};

  pluginlib::ClassLoader<nav2_core::WaypointTaskExecutor>
  waypoint_task_executor_loader_;
  pluginlib::UniquePtr<nav2_core::WaypointTaskExecutor>
  waypoint_task_executor_;
  std::string waypoint_task_executor_id_;
  std::string waypoint_task_executor_type_;
};

}  // namespace wheeltec_nav2

#endif  // wheeltec_nav2__WAYPOINT_FOLLOWER_WITH_WAIT_HPP_
