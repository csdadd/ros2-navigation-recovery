#pragma once

#include <QThread>
#include <QImage>
#include <QPointF>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/path.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include "coordinatetransformer.h"
#include "geometryutils.h"
#include "renderdata.h"

class Nav2ViewDataProcessor : public QThread {
    Q_OBJECT

signals:
    void renderDataReady(const RenderData& data);
    void mapLoadFailed(const QString& error);
    void mapLoadSucceeded(const QImage& map_image, const CoordinateTransformer& transformer);

public slots:
    void onGoalCleared();

public:
    // 仅更新渲染数据，不发布到话题（用于目标点预览）
    void updateGoalPoseOnly(double x, double y, double yaw);

public:
    explicit Nav2ViewDataProcessor(const std::string& map_yaml_path, QObject* parent = nullptr);
    ~Nav2ViewDataProcessor();

    void stopProcessing();

protected:
    void run() override;

private:
    bool loadMapFromYaml(const std::string& yaml_path);

    void planCallback(const nav_msgs::msg::Path::SharedPtr msg);
    void amclPoseCallback(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg);
    void goalPoseCallback(const geometry_msgs::msg::PoseStamped::SharedPtr msg);
    void localPlanCallback(const nav_msgs::msg::Path::SharedPtr msg);
    void scanCallback(const sensor_msgs::msg::LaserScan::SharedPtr msg);

private:
    std::string map_yaml_path_;

    // ROS 成员
    rclcpp::Node::SharedPtr node_;
    rclcpp::Executor::SharedPtr executor_;

    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr plan_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr amcl_pose_sub_;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr goal_pose_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goal_pose_pub_;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr local_plan_sub_;
    rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr scan_sub_;

    // TF
    std::unique_ptr<tf2_ros::Buffer> tf_buffer_;
    std::unique_ptr<tf2_ros::TransformListener> tf_listener_;

    // 渲染数据
    RenderData render_data_;
    std::mutex data_mutex_;

    // 坐标转换器
    CoordinateTransformer coord_transformer_{0.05, 0.0, 0.0, 0};

    // 地图数据
    QImage map_image_;
    double map_resolution_;
    double map_origin_x_;
    double map_origin_y_;
    bool map_loaded_;

    // 控制标志
    std::atomic<bool> should_stop_;
    bool goal_cleared_manually_;

    // 用于回调中访问机器人位置
    double callback_robot_x_;
    double callback_robot_y_;
    double callback_robot_yaw_;
};
