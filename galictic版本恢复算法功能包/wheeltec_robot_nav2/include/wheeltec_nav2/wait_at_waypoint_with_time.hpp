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

#ifndef wheeltec_nav2__WAIT_AT_WAYPOINT_WITH_TIME_HPP_
#define wheeltec_nav2__WAIT_AT_WAYPOINT_WITH_TIME_HPP_

#include <string>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"
#include "nav2_core/waypoint_task_executor.hpp"

namespace wheeltec_nav2
{

// 等待状态枚举
enum class WaitStatus
{
  RUNNING = 0,
  SUCCEEDED = 1,
  FAILED = 2
};

class WaitAtWaypointWithTime : public nav2_core::WaypointTaskExecutor
{
public:
  WaitAtWaypointWithTime();
  ~WaitAtWaypointWithTime();

  void initialize(
    const rclcpp_lifecycle::LifecycleNode::WeakPtr & parent,
    const std::string & plugin_name);

  bool processAtWaypoint(
    const geometry_msgs::msg::PoseStamped & curr_pose, const int & curr_waypoint_index);

  void setWaitTime(int32_t wait_time_ms);

  // 非阻塞接口
  void prepareWait(
    const geometry_msgs::msg::PoseStamped & curr_pose,
    const int & curr_waypoint_index,
    int32_t wait_time_ms);

  WaitStatus checkWaitComplete();

protected:
  int32_t current_wait_time_ms_;
  bool is_enabled_;
  rclcpp::Logger logger_{rclcpp::get_logger("wheeltec_nav2")};
  rclcpp::Clock::SharedPtr clock_;

  // 非阻塞等待相关成员
  rclcpp::Time wait_end_time_;
  bool is_waiting_{false};
};

}  // namespace wheeltec_nav2

#endif  // wheeltec_nav2__WAIT_AT_WAYPOINT_WITH_TIME_HPP_
