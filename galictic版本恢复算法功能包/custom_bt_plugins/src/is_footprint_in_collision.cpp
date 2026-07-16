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

#include "custom_bt_plugins/is_footprint_in_collision.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <memory>

#include "nav2_costmap_2d/cost_values.hpp"
#include "nav2_costmap_2d/footprint.hpp"
#include "nav2_costmap_2d/footprint_collision_checker.hpp"
#include "nav2_util/robot_utils.hpp"
#include "tf2_geometry_msgs/tf2_geometry_msgs.h"
#include "behaviortree_cpp_v3/bt_factory.h"

using nav2_costmap_2d::INSCRIBED_INFLATED_OBSTACLE;
using nav2_costmap_2d::LETHAL_OBSTACLE;
using nav2_costmap_2d::FREE_SPACE;
using nav2_costmap_2d::NO_INFORMATION;

namespace custom_bt_plugins
{

IsFootprintInCollision::IsFootprintInCollision(
  const std::string & name,
  const BT::NodeConfiguration & config)
: BT::ConditionNode(name, config),
  initialized_(false)
{
  initialize();
}

IsFootprintInCollision::~IsFootprintInCollision()
{
  callback_group_executor_.cancel();
  if (executor_thread_.joinable()) {
    executor_thread_.join();
  }
}

void IsFootprintInCollision::initialize()
{
  // 从黑板获取节点
  node_ = config().blackboard->get<rclcpp::Node::SharedPtr>("node");

  // 创建回调组和执行器
  callback_group_ = node_->create_callback_group(
    rclcpp::CallbackGroupType::MutuallyExclusive, false);
  callback_group_executor_.add_callback_group(
    callback_group_, node_->get_node_base_interface());

  // 启动执行器线程
  executor_thread_ = std::thread([this]() {callback_group_executor_.spin();});

  // 获取 TF buffer 和 listener
  tf_buffer_ = config().blackboard->get<std::shared_ptr<tf2_ros::Buffer>>("tf_buffer");
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  // 获取端口参数
  getInput("costmap_topic", costmap_topic_);
  getInput("collision_threshold", collision_threshold_);
  getInput("footprint_topic", footprint_topic_);

  // 如果阈值未设置，使用 INSCRIBED_INFLATED_OBSTACLE
  if (collision_threshold_ < 0) {
    collision_threshold_ = static_cast<double>(INSCRIBED_INFLATED_OBSTACLE);
  }

  // 订阅 costmap
  costmap_sub_ = std::make_shared<nav2_costmap_2d::CostmapSubscriber>(
    node_, costmap_topic_);

  // 订阅 footprint
  rclcpp::SubscriptionOptions sub_options;
  sub_options.callback_group = callback_group_;
  footprint_sub_ = node_->create_subscription<geometry_msgs::msg::PolygonStamped>(
    footprint_topic_,
    rclcpp::SystemDefaultsQoS(),
    std::bind(&IsFootprintInCollision::footprintCallback, this, std::placeholders::_1),
    sub_options);

  initialized_ = true;
  RCLCPP_INFO(
    node_->get_logger(),
    "[IsFootprintInCollision] 初始化完成，costmap话题: %s，footprint话题: %s，碰撞阈值: %.1f",
    costmap_topic_.c_str(), footprint_topic_.c_str(), collision_threshold_);
}

void IsFootprintInCollision::footprintCallback(
  const geometry_msgs::msg::PolygonStamped::SharedPtr msg)
{
  std::lock_guard<std::mutex> lock(footprint_mutex_);
  footprint_.clear();
  for (const auto & point : msg->polygon.points) {
    geometry_msgs::msg::Point pt;
    pt.x = point.x;
    pt.y = point.y;
    pt.z = point.z;
    footprint_.push_back(pt);
  }
}

bool IsFootprintInCollision::getCurrentPose(geometry_msgs::msg::Pose2D & pose)
{
  geometry_msgs::msg::PoseStamped current_pose;
  std::string base_frame = "base_link";
  std::string map_frame = "map";

  try {
    // 使用 nav2_util 获取当前位姿，transform_timeout 为 double 类型
    if (!nav2_util::getCurrentPose(
        current_pose, *tf_buffer_, map_frame, base_frame, 0.5))
    {
      RCLCPP_DEBUG(node_->get_logger(), "[IsFootprintInCollision] 无法获取当前位姿");
      return false;
    }
  } catch (const tf2::TransformException & ex) {
    RCLCPP_DEBUG(
      node_->get_logger(),
      "[IsFootprintInCollision] TF 异常: %s", ex.what());
    return false;
  }

  pose.x = current_pose.pose.position.x;
  pose.y = current_pose.pose.position.y;

  // 从四元数提取 yaw
  double roll, pitch, yaw;
  tf2::Quaternion q;
  tf2::fromMsg(current_pose.pose.orientation, q);
  tf2::Matrix3x3(q).getRPY(roll, pitch, yaw);
  pose.theta = yaw;

  return true;
}

double IsFootprintInCollision::getFootprintCost(
  const std::vector<geometry_msgs::msg::Point> & oriented_footprint)
{
  std::lock_guard<std::mutex> lock(costmap_mutex_);

  std::shared_ptr<nav2_costmap_2d::Costmap2D> costmap;
  try {
    costmap = costmap_sub_->getCostmap();
  } catch (const std::runtime_error & e) {
    RCLCPP_DEBUG(node_->get_logger(), "[IsFootprintInCollision] costmap 未就绪: %s", e.what());
    return -1.0;
  }
  if (!costmap) {
    RCLCPP_DEBUG(node_->get_logger(), "[IsFootprintInCollision] costmap 为空");
    return -1.0;
  }

  // 创建碰撞检测器
  nav2_costmap_2d::FootprintCollisionChecker<nav2_costmap_2d::Costmap2D *> checker(costmap.get());

  // 计算 footprint 代价
  return checker.footprintCost(oriented_footprint);
}

BT::NodeStatus IsFootprintInCollision::tick()
{
  if (!initialized_) {
    RCLCPP_WARN(node_->get_logger(), "[IsFootprintInCollision] 节点未初始化");
    return BT::NodeStatus::FAILURE;
  }

  // 获取当前 footprint
  std::vector<geometry_msgs::msg::Point> current_footprint;
  {
    std::lock_guard<std::mutex> lock(footprint_mutex_);
    current_footprint = footprint_;
  }

  if (current_footprint.empty()) {
    RCLCPP_DEBUG(node_->get_logger(), "[IsFootprintInCollision] footprint 未就绪");
    return BT::NodeStatus::FAILURE;
  }

  // 获取当前位姿
  geometry_msgs::msg::Pose2D current_pose;
  if (!getCurrentPose(current_pose)) {
    return BT::NodeStatus::FAILURE;
  }

  // 计算旋转后的 footprint
  double cos_th = cos(current_pose.theta);
  double sin_th = sin(current_pose.theta);

  std::vector<geometry_msgs::msg::Point> oriented_footprint;
  oriented_footprint.reserve(current_footprint.size());

  for (const auto & pt : current_footprint) {
    geometry_msgs::msg::Point new_pt;
    new_pt.x = current_pose.x + (pt.x * cos_th - pt.y * sin_th);
    new_pt.y = current_pose.y + (pt.x * sin_th + pt.y * cos_th);
    new_pt.z = 0.0;
    oriented_footprint.push_back(new_pt);
  }

  // 计算 footprint 代价
  double footprint_cost = getFootprintCost(oriented_footprint);

  if (footprint_cost < 0) {
    RCLCPP_DEBUG(node_->get_logger(), "[IsFootprintInCollision] 无法计算 footprint 代价");
    return BT::NodeStatus::FAILURE;
  }

  RCLCPP_DEBUG(
    node_->get_logger(),
    "[IsFootprintInCollision] 位姿 (%.2f, %.2f, %.2f)，footprint 代价: %.1f",
    current_pose.x, current_pose.y, current_pose.theta, footprint_cost);

  // 判断是否碰撞
  if (footprint_cost >= collision_threshold_) {
    RCLCPP_INFO(
      node_->get_logger(),
      "[IsFootprintInCollision] 检测到碰撞！footprint 代价: %.1f >= 阈值: %.1f",
      footprint_cost, collision_threshold_);
    return BT::NodeStatus::SUCCESS;  // 碰撞
  }

  return BT::NodeStatus::FAILURE;  // 无碰撞
}

}  // namespace custom_bt_plugins

// 注册节点
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<custom_bt_plugins::IsFootprintInCollision>("IsFootprintInCollision");
}
