// Copyright (c) 2020 Fetullah Atas
// Modified for dynamic wait times
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

#include "robot_navigation2/wait_at_waypoint_with_time.hpp"

#include <string>
#include <exception>

#include "pluginlib/class_list_macros.hpp"
#include "nav2_util/node_utils.hpp"

namespace robot_navigation2
{

WaitAtWaypointWithTime::WaitAtWaypointWithTime()
: current_wait_time_ms_(0),
  is_enabled_(true)
{
}

WaitAtWaypointWithTime::~WaitAtWaypointWithTime()
{
}

void WaitAtWaypointWithTime::initialize(
  const rclcpp_lifecycle::LifecycleNode::WeakPtr & parent,
  const std::string & plugin_name)
{
  auto node = parent.lock();
  if (!node) {
    throw std::runtime_error{"Failed to lock node in wait at waypoint plugin!"};
  }
  logger_ = node->get_logger();
  clock_ = node->get_clock();

  nav2_util::declare_parameter_if_not_declared(
    node,
    plugin_name + ".enabled",
    rclcpp::ParameterValue(true));
  node->get_parameter(
    plugin_name + ".enabled",
    is_enabled_);

  if (!is_enabled_) {
    RCLCPP_INFO(logger_, "Waypoint task executor plugin is disabled.");
  }
}

void WaitAtWaypointWithTime::setWaitTime(int32_t wait_time_ms)
{
  current_wait_time_ms_ = wait_time_ms;
}

void WaitAtWaypointWithTime::prepareWait(
  const geometry_msgs::msg::PoseStamped & /*curr_pose*/,
  const int & curr_waypoint_index,
  int32_t wait_time_ms)
{
  if (!is_enabled_ || wait_time_ms <= 0) {
    RCLCPP_INFO(
      logger_, "Waypoint %i wait time is %d ms, skipping wait.",
      curr_waypoint_index, wait_time_ms);
    is_waiting_ = false;
    return;
  }

  RCLCPP_INFO(
    logger_, "Preparing to wait at %i'th waypoint for %d milliseconds",
    curr_waypoint_index, wait_time_ms);

  wait_end_time_ = clock_->now() +
    rclcpp::Duration::from_nanoseconds(wait_time_ms * 1000000LL);
  is_waiting_ = true;
}

WaitStatus WaitAtWaypointWithTime::checkWaitComplete()
{
  if (!is_waiting_) {
    return WaitStatus::SUCCEEDED;
  }

  if (clock_->now() >= wait_end_time_) {
    is_waiting_ = false;
    RCLCPP_INFO(logger_, "Wait completed");
    return WaitStatus::SUCCEEDED;
  }

  return WaitStatus::RUNNING;
}

bool WaitAtWaypointWithTime::processAtWaypoint(
  const geometry_msgs::msg::PoseStamped & /*curr_pose*/, const int & curr_waypoint_index)
{
  // 保持兼容性的空实现，实际等待由非阻塞接口处理
  if (!is_enabled_) {
    return true;
  }

  if (current_wait_time_ms_ <= 0) {
    RCLCPP_INFO(
      logger_, "Waypoint %i wait time is %d ms, skipping wait.",
      curr_waypoint_index, current_wait_time_ms_);
    return true;
  }

  RCLCPP_INFO(
    logger_, "Arrived at %i'th waypoint, sleeping for %d milliseconds",
    curr_waypoint_index,
    current_wait_time_ms_);
  clock_->sleep_for(std::chrono::milliseconds(current_wait_time_ms_));
  return true;
}

}  // namespace robot_navigation2

PLUGINLIB_EXPORT_CLASS(
  robot_navigation2::WaitAtWaypointWithTime,
  nav2_core::WaypointTaskExecutor)
