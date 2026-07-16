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

#ifndef CUSTOM_BT_PLUGINS__IS_FOOTPRINT_IN_COLLISION_HPP_
#define CUSTOM_BT_PLUGINS__IS_FOOTPRINT_IN_COLLISION_HPP_

#include <string>
#include <memory>
#include <vector>
#include <mutex>

#include "behaviortree_cpp_v3/condition_node.h"
#include "nav2_costmap_2d/footprint_collision_checker.hpp"
#include "nav2_costmap_2d/costmap_subscriber.hpp"
#include "nav2_util/robot_utils.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "nav_msgs/msg/occupancy_grid.hpp"
#include "geometry_msgs/msg/point.hpp"
#include "geometry_msgs/msg/pose2_d.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"

namespace custom_bt_plugins
{

/**
 * @brief BT 节点：检测机器人 footprint 是否与障碍物重叠
 *
 * 当 footprint 代价 >= INSCRIBED_INFLATED_OBSTACLE 时返回 SUCCESS（碰撞）
 * 否则返回 FAILURE（无碰撞）
 */
class IsFootprintInCollision : public BT::ConditionNode
{
public:
  /**
   * @brief 构造函数
   * @param name 节点名称
   * @param config BT 节点配置
   */
  IsFootprintInCollision(
    const std::string & name,
    const BT::NodeConfiguration & config);

  /**
   * @brief 析构函数
   */
  ~IsFootprintInCollision() override;

  /**
   * @brief 定义节点端口
   */
  static BT::PortsList providedPorts()
  {
    return {
      BT::InputPort<std::string>(
        "costmap_topic", "/local_costmap/costmap_raw", "代价地图话题"),
      BT::InputPort<double>(
        "collision_threshold", -1.0, "碰撞阈值，-1表示使用INSCRIBED_INFLATED_OBSTACLE"),
      BT::InputPort<std::string>(
        "footprint_topic", "/local_costmap/published_footprint", "footprint话题"),
    };
  }

  /**
   * @brief 执行节点逻辑
   */
  BT::NodeStatus tick() override;

private:
  /**
   * @brief 初始化节点
   */
  void initialize();

  /**
   * @brief 获取机器人当前位姿
   * @param pose 输出位姿
   * @return 是否成功获取
   */
  bool getCurrentPose(geometry_msgs::msg::Pose2D & pose);

  /**
   * @brief 从话题获取 footprint
   * @param msg footprint 消息
   */
  void footprintCallback(const geometry_msgs::msg::PolygonStamped::SharedPtr msg);

  /**
   * @brief 计算 footprint 在 costmap 中的最大代价
   * @param oriented_footprint 旋转后的 footprint
   * @return 最大代价值
   */
  double getFootprintCost(const std::vector<geometry_msgs::msg::Point> & oriented_footprint);

  rclcpp::Node::SharedPtr node_;
  std::shared_ptr<tf2_ros::Buffer> tf_buffer_;
  std::shared_ptr<tf2_ros::TransformListener> tf_listener_;

  std::string costmap_topic_;
  double collision_threshold_;
  std::string footprint_topic_;

  std::shared_ptr<nav2_costmap_2d::CostmapSubscriber> costmap_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PolygonStamped>::SharedPtr footprint_sub_;

  std::vector<geometry_msgs::msg::Point> footprint_;
  std::mutex footprint_mutex_;

  bool initialized_;
  rclcpp::CallbackGroup::SharedPtr callback_group_;
  rclcpp::executors::SingleThreadedExecutor callback_group_executor_;
  std::thread executor_thread_;
};

}  // namespace custom_bt_plugins

#endif  // CUSTOM_BT_PLUGINS__IS_FOOTPRINT_IN_COLLISION_HPP_
