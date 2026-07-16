#include "nav2viewdataprocessor.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <functional>

Nav2ViewDataProcessor::Nav2ViewDataProcessor(const std::string& map_yaml_path, QObject* parent)
    : QThread(parent)
    , map_yaml_path_(map_yaml_path)
    , map_resolution_(0.05)
    , map_origin_x_(0.0)
    , map_origin_y_(0.0)
    , map_loaded_(false)
    , should_stop_(false)
    , goal_cleared_manually_(false)
    , callback_robot_x_(0.0)
    , callback_robot_y_(0.0)
    , callback_robot_yaw_(0.0)
{
}

Nav2ViewDataProcessor::~Nav2ViewDataProcessor() {
    stopProcessing();
    wait();
}

void Nav2ViewDataProcessor::stopProcessing() {
    should_stop_ = true;
}

void Nav2ViewDataProcessor::run() {
    // ROS context 由 ROSContextManager 统一管理，此处不再初始化

    // 创建节点
    node_ = std::make_shared<rclcpp::Node>("nav2_view_data_processor");

    // 创建 executor
    executor_ = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();

    // 加载地图
    if (!loadMapFromYaml(map_yaml_path_)) {
        QString error_msg = QString("Failed to load map from: %1").arg(QString::fromStdString(map_yaml_path_));
        qWarning() << error_msg;
        Q_EMIT mapLoadFailed(error_msg);
        return;
    }
    Q_EMIT mapLoadSucceeded(map_image_, coord_transformer_);

    // 创建订阅者
    plan_sub_ = node_->create_subscription<nav_msgs::msg::Path>(
        "/plan", 10, std::bind(&Nav2ViewDataProcessor::planCallback, this, std::placeholders::_1));

    amcl_pose_sub_ = node_->create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
        "/amcl_pose", 10, std::bind(&Nav2ViewDataProcessor::amclPoseCallback, this, std::placeholders::_1));

    goal_pose_sub_ = node_->create_subscription<geometry_msgs::msg::PoseStamped>(
        "/goal_pose", 10, std::bind(&Nav2ViewDataProcessor::goalPoseCallback, this, std::placeholders::_1));

    local_plan_sub_ = node_->create_subscription<nav_msgs::msg::Path>(
        "/local_plan", 10, std::bind(&Nav2ViewDataProcessor::localPlanCallback, this, std::placeholders::_1));

    scan_sub_ = node_->create_subscription<sensor_msgs::msg::LaserScan>(
        "/scan", 10, std::bind(&Nav2ViewDataProcessor::scanCallback, this, std::placeholders::_1));

    // 初始化 TF2
    tf_buffer_ = std::make_unique<tf2_ros::Buffer>(node_->get_clock());
    tf_listener_ = std::make_unique<tf2_ros::TransformListener>(*tf_buffer_);

    // 添加节点到 executor
    executor_->add_node(node_);

    // 事件循环
    while (!should_stop_) {
        executor_->spin_some(std::chrono::milliseconds(10));

        // 发射渲染数据
        {
            std::lock_guard<std::mutex> lock(data_mutex_);
            RenderData data_copy = render_data_;
            Q_EMIT renderDataReady(data_copy);
        }

        QThread::msleep(30);  // 约 30fps
    }

    // 清理
    executor_.reset();
    node_.reset();
    tf_listener_.reset();
    tf_buffer_.reset();

    // ROS shutdown 由 ROSContextManager 统一管理，此处不再调用
}

bool Nav2ViewDataProcessor::loadMapFromYaml(const std::string& yaml_path) {
    try {
        YAML::Node config = YAML::LoadFile(yaml_path);

        if (!config["image"]) {
            qWarning() << "YAML missing 'image' field";
            return false;
        }

        std::string image_name = config["image"].as<std::string>();

        std::string yaml_dir = yaml_path.substr(0, yaml_path.find_last_of("/\\"));
        std::string image_path = yaml_dir + "/" + image_name;

        if (!map_image_.load(QString::fromStdString(image_path))) {
            qWarning() << "Failed to load image:" << QString::fromStdString(image_path);
            return false;
        }

        qDebug() << "[Nav2ViewDataProcessor] 地图加载成功:" << QString::fromStdString(image_path);
        qDebug() << "[Nav2ViewDataProcessor] 地图尺寸:" << map_image_.width() << "x" << map_image_.height();
        qDebug() << "[Nav2ViewDataProcessor] 原始像素格式:" << map_image_.format();

        // 转换为 RGB 格式以确保正确显示
        if (map_image_.format() == QImage::Format_Indexed8) {
            map_image_ = map_image_.convertToFormat(QImage::Format_RGB32);
            qDebug() << "[Nav2ViewDataProcessor] 已转换为 RGB32 格式";
        }

        if (config["resolution"]) {
            map_resolution_ = config["resolution"].as<double>();
        }

        if (config["origin"]) {
            auto origin = config["origin"];
            if (origin.IsSequence() && origin.size() >= 2) {
                map_origin_x_ = origin[0].as<double>();
                map_origin_y_ = origin[1].as<double>();
            }
        }

        // 初始化坐标转换器
        coord_transformer_ = CoordinateTransformer(map_resolution_, map_origin_x_, map_origin_y_, map_image_.height());

        qDebug() << "[Nav2ViewDataProcessor] 地图分辨率:" << map_resolution_;
        qDebug() << "[Nav2ViewDataProcessor] 地图原点:" << map_origin_x_ << "," << map_origin_y_;

        map_loaded_ = true;
        return true;

    } catch (const YAML::Exception& e) {
        qWarning() << "YAML parsing error:" << e.what();
        return false;
    }
}

void Nav2ViewDataProcessor::planCallback(const nav_msgs::msg::Path::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    render_data_.path_points.clear();
    for (const auto& pose_stamped : msg->poses) {
        QPointF pt = coord_transformer_.mapToQt(pose_stamped.pose.position.x, pose_stamped.pose.position.y);
        render_data_.path_points.push_back(pt);
    }
}

void Nav2ViewDataProcessor::amclPoseCallback(const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    callback_robot_x_ = msg->pose.pose.position.x;
    callback_robot_y_ = msg->pose.pose.position.y;
    callback_robot_yaw_ = GeometryUtils::quaternionToYaw(msg->pose.pose.orientation);

    render_data_.robot_x = callback_robot_x_;
    render_data_.robot_y = callback_robot_y_;
    render_data_.robot_yaw = callback_robot_yaw_;
    render_data_.robot_pose_received = true;
}

void Nav2ViewDataProcessor::goalPoseCallback(const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
    // 如果用户手动清除了目标，忽略来自话题的更新
    if (goal_cleared_manually_) {
        return;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    render_data_.goal_x = msg->pose.position.x;
    render_data_.goal_y = msg->pose.position.y;
    render_data_.goal_yaw = GeometryUtils::quaternionToYaw(msg->pose.orientation);
    render_data_.goal_pose_received = true;
}

void Nav2ViewDataProcessor::localPlanCallback(const nav_msgs::msg::Path::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    render_data_.local_path_points.clear();

    std::string frame_id = msg->header.frame_id;

    // 如果已经是 map 坐标系，直接使用原始坐标
    if (frame_id.find("map") != std::string::npos) {
        for (const auto& pose_stamped : msg->poses) {
            QPointF pt = coord_transformer_.mapToQt(
                pose_stamped.pose.position.x,
                pose_stamped.pose.position.y);
            render_data_.local_path_points.push_back(pt);
        }
    }
    // odom 或其他坐标系需要使用 TF2 转换为 map 坐标系
    else {
        geometry_msgs::msg::TransformStamped transform;
        bool transform_available = false;

        try {
            // 查找从路径坐标系到 map 的变换
            transform = tf_buffer_->lookupTransform(
                "map", frame_id, msg->header.stamp, rclcpp::Duration::from_seconds(0.1));
            transform_available = true;
        } catch (const tf2::TransformException& ex) {
            // 如果特定时间戳的变换不可用，尝试最新可用变换
            try {
                transform = tf_buffer_->lookupTransform("map", frame_id, tf2::TimePointZero);
                transform_available = true;
            } catch (const tf2::TransformException& ex2) {
                qDebug() << "[Nav2ViewDataProcessor] 无法获取从" << QString::fromStdString(frame_id)
                         << "到 map 的变换:" << ex2.what();
            }
        }

        for (const auto& pose_stamped : msg->poses) {
            double x = pose_stamped.pose.position.x;
            double y = pose_stamped.pose.position.y;

            if (transform_available) {
                // 使用 TF2 进行正确的坐标变换
                geometry_msgs::msg::PoseStamped pose_in, pose_out;
                pose_in.pose = pose_stamped.pose;
                pose_in.header = pose_stamped.header;

                tf2::doTransform(pose_in, pose_out, transform);
                x = pose_out.pose.position.x;
                y = pose_out.pose.position.y;
            } else {
                // 回退方案：使用机器人位置进行近似变换（仅平移）
                x += callback_robot_x_;
                y += callback_robot_y_;
            }

            QPointF pt = coord_transformer_.mapToQt(x, y);
            render_data_.local_path_points.push_back(pt);
        }
    }
}

void Nav2ViewDataProcessor::scanCallback(const sensor_msgs::msg::LaserScan::SharedPtr msg) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    render_data_.scan_points.clear();

    std::string frame_id = msg->header.frame_id;
    static int scanCount = 0;
    scanCount++;
    if (scanCount % 10 == 0) {
        qDebug() << "[Nav2ViewDataProcessor] 激光坐标系:" << QString::fromStdString(frame_id)
                 << "机器人位置:" << callback_robot_x_ << callback_robot_y_ << "航向:" << callback_robot_yaw_;
    }

    float angle = msg->angle_min;
    for (const auto& range : msg->ranges) {
        if (range >= msg->range_min && range <= msg->range_max &&
            std::isfinite(range)) {

            // 激光坐标系下的坐标（相对于激光雷达）
            float laser_x = range * std::cos(angle);
            float laser_y = range * std::sin(angle);

            // 转换为地图坐标系：激光 -> 机器人 -> 地图
            double map_x = callback_robot_x_ + laser_x * std::cos(callback_robot_yaw_) - laser_y * std::sin(callback_robot_yaw_);
            double map_y = callback_robot_y_ + laser_x * std::sin(callback_robot_yaw_) + laser_y * std::cos(callback_robot_yaw_);

            QPointF pt = coord_transformer_.mapToQt(map_x, map_y);
            render_data_.scan_points.push_back(pt);
        }
        angle += msg->angle_increment;
    }
}

void Nav2ViewDataProcessor::onGoalCleared() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    render_data_.goal_x = 0.0;
    render_data_.goal_y = 0.0;
    render_data_.goal_yaw = 0.0;
    render_data_.goal_pose_received = false;
    goal_cleared_manually_ = true;
}

void Nav2ViewDataProcessor::updateGoalPoseOnly(double x, double y, double yaw)
{
    // 仅更新本地渲染数据，不发布到话题（用于目标点预览）
    std::lock_guard<std::mutex> lock(data_mutex_);
    render_data_.goal_x = x;
    render_data_.goal_y = y;
    render_data_.goal_yaw = yaw;
    render_data_.goal_pose_received = true;
}
