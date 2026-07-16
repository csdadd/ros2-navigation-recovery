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

#include "robot_navigation2/waypoint_follower_with_wait.hpp"
#include "robot_navigation2/wait_at_waypoint_with_time.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace robot_navigation2
{

using rcl_interfaces::msg::ParameterType;
using std::placeholders::_1;

WaypointFollowerWithWait::WaypointFollowerWithWait(const rclcpp::NodeOptions & options)
: nav2_util::LifecycleNode("waypoint_follower_with_wait", "", options),
  waypoint_task_executor_loader_("nav2_waypoint_follower",
    "nav2_core::WaypointTaskExecutor")
{
  RCLCPP_INFO(get_logger(), "Creating");

  declare_parameter("stop_on_failure", true);
  declare_parameter("loop_rate", 20);
  nav2_util::declare_parameter_if_not_declared(
    this, std::string("waypoint_task_executor_plugin"),
    rclcpp::ParameterValue(std::string("wait_at_waypoint_with_time")));
  nav2_util::declare_parameter_if_not_declared(
    this, std::string("wait_at_waypoint_with_time.plugin"),
    rclcpp::ParameterValue(std::string("robot_navigation2::WaitAtWaypointWithTime")));
}

WaypointFollowerWithWait::~WaypointFollowerWithWait()
{
}

nav2_util::CallbackReturn
WaypointFollowerWithWait::on_configure(const rclcpp_lifecycle::State & /*state*/)
{
  RCLCPP_INFO(get_logger(), "Configuring");

  auto node = shared_from_this();

  stop_on_failure_ = get_parameter("stop_on_failure").as_bool();
  loop_rate_ = get_parameter("loop_rate").as_int();
  waypoint_task_executor_id_ = get_parameter("waypoint_task_executor_plugin").as_string();

  callback_group_ = create_callback_group(
    rclcpp::CallbackGroupType::MutuallyExclusive,
    false);
  callback_group_executor_.add_callback_group(callback_group_, get_node_base_interface());

  nav_to_pose_client_ = rclcpp_action::create_client<ClientT>(
    get_node_base_interface(),
    get_node_graph_interface(),
    get_node_logging_interface(),
    get_node_waitables_interface(),
    "navigate_to_pose", callback_group_);

  action_server_ = std::make_unique<ActionServer>(
    get_node_base_interface(),
    get_node_clock_interface(),
    get_node_logging_interface(),
    get_node_waitables_interface(),
    "follow_waypoints_with_wait", std::bind(&WaypointFollowerWithWait::followWaypoints, this));

  try {
    waypoint_task_executor_type_ = nav2_util::get_plugin_type_param(
      this,
      waypoint_task_executor_id_);
    waypoint_task_executor_ = waypoint_task_executor_loader_.createUniqueInstance(
      waypoint_task_executor_type_);
    RCLCPP_INFO(
      get_logger(), "Created waypoint_task_executor : %s of type %s",
      waypoint_task_executor_id_.c_str(), waypoint_task_executor_type_.c_str());
    waypoint_task_executor_->initialize(node, waypoint_task_executor_id_);
  } catch (const pluginlib::PluginlibException & ex) {
    RCLCPP_FATAL(
      get_logger(),
      "Failed to create waypoint_task_executor. Exception: %s", ex.what());
  }

  return nav2_util::CallbackReturn::SUCCESS;
}

nav2_util::CallbackReturn
WaypointFollowerWithWait::on_activate(const rclcpp_lifecycle::State & /*state*/)
{
  RCLCPP_INFO(get_logger(), "Activating");

  action_server_->activate();

  auto node = shared_from_this();
  dyn_params_handler_ = node->add_on_set_parameters_callback(
    std::bind(&WaypointFollowerWithWait::dynamicParametersCallback, this, _1));

  createBond();

  return nav2_util::CallbackReturn::SUCCESS;
}

nav2_util::CallbackReturn
WaypointFollowerWithWait::on_deactivate(const rclcpp_lifecycle::State & /*state*/)
{
  RCLCPP_INFO(get_logger(), "Deactivating");

  action_server_->deactivate();
  dyn_params_handler_.reset();

  destroyBond();

  return nav2_util::CallbackReturn::SUCCESS;
}

nav2_util::CallbackReturn
WaypointFollowerWithWait::on_cleanup(const rclcpp_lifecycle::State & /*state*/)
{
  RCLCPP_INFO(get_logger(), "Cleaning up");

  action_server_.reset();
  nav_to_pose_client_.reset();

  return nav2_util::CallbackReturn::SUCCESS;
}

nav2_util::CallbackReturn
WaypointFollowerWithWait::on_shutdown(const rclcpp_lifecycle::State & /*state*/)
{
  RCLCPP_INFO(get_logger(), "Shutting down");
  return nav2_util::CallbackReturn::SUCCESS;
}

void
WaypointFollowerWithWait::followWaypoints()
{
  auto goal = action_server_->get_current_goal();
  auto feedback = std::make_shared<ActionT::Feedback>();
  auto result = std::make_shared<ActionT::Result>();

  if (!action_server_ || !action_server_->is_server_active()) {
    RCLCPP_DEBUG(get_logger(), "Action server inactive. Stopping.");
    return;
  }

  RCLCPP_INFO(
    get_logger(), "Received follow waypoint request with %i waypoints.",
    static_cast<int>(goal->poses.size()));

  if (goal->poses.size() == 0) {
    action_server_->succeeded_current(result);
    return;
  }

  // Validate wait_times_ms array size
  if (goal->wait_times_ms.size() != goal->poses.size()) {
    RCLCPP_ERROR(
      get_logger(),
      "wait_times_ms size (%zu) does not match poses size (%zu)",
      goal->wait_times_ms.size(), goal->poses.size());
    result->missed_waypoints.clear();
    action_server_->terminate_current(result);
    return;
  }

  wait_times_ms_ = goal->wait_times_ms;

  rclcpp::WallRate r(loop_rate_);
  uint32_t goal_index = 0;
  executor_state_ = ExecutorState::IDLE;
  current_goal_status_ = ActionStatus::UNKNOWN;

  while (rclcpp::ok()) {
    // 检查取消请求
    if (action_server_->is_cancel_requested()) {
      auto cancel_future = nav_to_pose_client_->async_cancel_all_goals();
      callback_group_executor_.spin_until_future_complete(cancel_future);
      callback_group_executor_.spin_some();
      action_server_->terminate_all();
      executor_state_ = ExecutorState::IDLE;
      return;
    }

    // 检查抢占请求
    if (action_server_->is_preempt_requested()) {
      RCLCPP_INFO(get_logger(), "Preempting the goal pose.");
      goal = action_server_->accept_pending_goal();
      goal_index = 0;
      executor_state_ = ExecutorState::IDLE;
      current_goal_status_ = ActionStatus::UNKNOWN;

      if (goal->wait_times_ms.size() != goal->poses.size()) {
        RCLCPP_ERROR(
          get_logger(),
          "Preempted goal: wait_times_ms size (%zu) does not match poses size (%zu)",
          goal->wait_times_ms.size(), goal->poses.size());
        result->missed_waypoints.clear();
        action_server_->terminate_current(result);
        return;
      }
      wait_times_ms_ = goal->wait_times_ms;
    }

    // 状态机处理
    switch (executor_state_) {
      case ExecutorState::IDLE:
        {
          // 发送导航目标
          ClientT::Goal client_goal;
          client_goal.pose = goal->poses[goal_index];

          auto send_goal_options = rclcpp_action::Client<ClientT>::SendGoalOptions();
          send_goal_options.result_callback =
            std::bind(&WaypointFollowerWithWait::resultCallback, this, std::placeholders::_1);
          send_goal_options.goal_response_callback =
            std::bind(&WaypointFollowerWithWait::goalResponseCallback, this, std::placeholders::_1);
          future_goal_handle_ =
            nav_to_pose_client_->async_send_goal(client_goal, send_goal_options);
          current_goal_status_ = ActionStatus::PROCESSING;
          executor_state_ = ExecutorState::NAVIGATING;
        }
        break;

      case ExecutorState::NAVIGATING:
        // 等待导航完成
        if (current_goal_status_ == ActionStatus::FAILED) {
          failed_ids_.push_back(goal_index);

          if (stop_on_failure_) {
            RCLCPP_WARN(
              get_logger(), "Failed to process waypoint %i in waypoint "
              "list and stop on failure is enabled."
              " Terminating action.", goal_index);
            result->missed_waypoints = failed_ids_;
            action_server_->terminate_current(result);
            failed_ids_.clear();
            executor_state_ = ExecutorState::IDLE;
            return;
          } else {
            RCLCPP_INFO(
              get_logger(), "Failed to process waypoint %i,"
              " moving to next.", goal_index);
            goal_index++;
            executor_state_ = ExecutorState::IDLE;
          }
        } else if (current_goal_status_ == ActionStatus::SUCCEEDED) {
          RCLCPP_INFO(
            get_logger(), "Succeeded processing waypoint %i, preparing wait with time %d ms",
            goal_index, wait_times_ms_[goal_index]);

          // 初始化非阻塞等待
          auto wait_plugin = dynamic_cast<WaitAtWaypointWithTime *>(waypoint_task_executor_.get());
          if (wait_plugin) {
            wait_plugin->prepareWait(
              goal->poses[goal_index], goal_index, wait_times_ms_[goal_index]);
          }
          executor_state_ = ExecutorState::WAITING_AT_WAYPOINT;
        } else {
          // 仍在导航中，打印状态日志
          RCLCPP_INFO_EXPRESSION(
            get_logger(),
            (static_cast<int>(now().seconds()) % 30 == 0),
            "Navigating to waypoint %i...", goal_index);
        }
        break;

      case ExecutorState::WAITING_AT_WAYPOINT:
        {
          // 检查等待完成状态
          auto wait_plugin = dynamic_cast<WaitAtWaypointWithTime *>(waypoint_task_executor_.get());
          WaitStatus wait_status = WaitStatus::SUCCEEDED;
          if (wait_plugin) {
            wait_status = wait_plugin->checkWaitComplete();
          }

          if (wait_status == WaitStatus::SUCCEEDED) {
            RCLCPP_INFO(
              get_logger(), "Wait completed at waypoint %i, moving to next.", goal_index);
            goal_index++;
            executor_state_ = ExecutorState::IDLE;
          } else if (wait_status == WaitStatus::FAILED) {
            if (stop_on_failure_) {
              failed_ids_.push_back(goal_index);
              RCLCPP_WARN(
                get_logger(), "Failed to execute task at waypoint %i "
                " stop on failure is enabled."
                " Terminating action.", goal_index);
              result->missed_waypoints = failed_ids_;
              action_server_->terminate_current(result);
              failed_ids_.clear();
              executor_state_ = ExecutorState::IDLE;
              return;
            } else {
              RCLCPP_INFO(
                get_logger(), "Failed wait at waypoint %i, moving to next.", goal_index);
              goal_index++;
              executor_state_ = ExecutorState::IDLE;
            }
          }
          // RUNNING 状态继续等待
        }
        break;
    }

    // 检查是否完成所有航点
    if (goal_index >= goal->poses.size()) {
      RCLCPP_INFO(
        get_logger(), "Completed all %zu waypoints requested.",
        goal->poses.size());
      result->missed_waypoints = failed_ids_;
      action_server_->succeeded_current(result);
      failed_ids_.clear();
      executor_state_ = ExecutorState::IDLE;
      return;
    }

    // 发布反馈
    feedback->current_waypoint = goal_index;
    action_server_->publish_feedback(feedback);

    callback_group_executor_.spin_some();
    r.sleep();
  }
}

void
WaypointFollowerWithWait::resultCallback(
  const rclcpp_action::ClientGoalHandle<ClientT>::WrappedResult & result)
{
  if (result.goal_id != future_goal_handle_.get()->get_goal_id()) {
    RCLCPP_DEBUG(
      get_logger(),
      "Goal IDs do not match for the current goal handle and received result."
      "Ignoring likely due to receiving result for an old goal.");
    return;
  }

  switch (result.code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
      current_goal_status_ = ActionStatus::SUCCEEDED;
      return;
    case rclcpp_action::ResultCode::ABORTED:
      current_goal_status_ = ActionStatus::FAILED;
      return;
    case rclcpp_action::ResultCode::CANCELED:
      current_goal_status_ = ActionStatus::FAILED;
      return;
    default:
      current_goal_status_ = ActionStatus::UNKNOWN;
      return;
  }
}

void
WaypointFollowerWithWait::goalResponseCallback(
  const rclcpp_action::ClientGoalHandle<ClientT>::SharedPtr & goal)
{
  if (!goal) {
    RCLCPP_ERROR(
      get_logger(),
      "navigate_to_pose action client failed to send goal to server.");
    current_goal_status_ = ActionStatus::FAILED;
  }
}

rcl_interfaces::msg::SetParametersResult
WaypointFollowerWithWait::dynamicParametersCallback(std::vector<rclcpp::Parameter> parameters)
{
  rcl_interfaces::msg::SetParametersResult result;

  for (auto parameter : parameters) {
    const auto & type = parameter.get_type();
    const auto & name = parameter.get_name();

    if (type == ParameterType::PARAMETER_INTEGER) {
      if (name == "loop_rate") {
        loop_rate_ = parameter.as_int();
      }
    } else if (type == ParameterType::PARAMETER_BOOL) {
      if (name == "stop_on_failure") {
        stop_on_failure_ = parameter.as_bool();
      }
    }
  }

  result.successful = true;
  return result;
}

}  // namespace robot_navigation2

#include "rclcpp_components/register_node_macro.hpp"

RCLCPP_COMPONENTS_REGISTER_NODE(robot_navigation2::WaypointFollowerWithWait)
