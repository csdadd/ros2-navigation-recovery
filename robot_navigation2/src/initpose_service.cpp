#include "rclcpp/rclcpp.hpp"
#include "nav2_msgs/srv/set_initial_pose.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include <memory>
#include <cmath>

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = rclcpp::Node::make_shared("initpose_service_node");
    RCLCPP_INFO(node->get_logger(), "Initial pose service client node created");

    auto client = node->create_client<nav2_msgs::srv::SetInitialPose>("/set_initial_pose");

    while (!client->wait_for_service(std::chrono::seconds(1))) {
        if (!rclcpp::ok()) {
            RCLCPP_ERROR(node->get_logger(), "Interrupted while waiting for service");
            return 1;
        }
        RCLCPP_INFO(node->get_logger(), "Waiting for /set_initial_pose service...");
    }

    auto request = std::make_shared<nav2_msgs::srv::SetInitialPose::Request>();

    request->pose.header.stamp = node->now();
    request->pose.header.frame_id = "map";

    request->pose.pose.pose.position.x = 0;
    request->pose.pose.pose.position.y = 0;
    request->pose.pose.pose.position.z = 0.0;

    double qz = 0.0;
    double qw = 1.0;
    double norm = std::sqrt(qz * qz + qw * qw);
    request->pose.pose.pose.orientation.x = 0.0;
    request->pose.pose.pose.orientation.y = 0.0;
    request->pose.pose.pose.orientation.z = qz / norm;
    request->pose.pose.pose.orientation.w = qw / norm;

    auto result_future = client->async_send_request(request);

    if (rclcpp::spin_until_future_complete(node, result_future) == rclcpp::FutureReturnCode::SUCCESS) {
        RCLCPP_INFO(node->get_logger(), "Initial pose set successfully via service");
    } else {
        RCLCPP_ERROR(node->get_logger(), "Failed to set initial pose");
    }

    rclcpp::shutdown();
    return 0;
}
