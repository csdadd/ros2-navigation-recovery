#pragma once

#include <QPointF>
#include <vector>

struct RenderData {
    // 机器人类
    double robot_x = 0.0;
    double robot_y = 0.0;
    double robot_yaw = 0.0;
    bool robot_pose_received = false;

    // 路径（坐标已转换为 Qt 坐标）
    std::vector<QPointF> path_points;
    std::vector<QPointF> local_path_points;

    // 激光点云（坐标已转换为 Qt 坐标）
    std::vector<QPointF> scan_points;

    // 目标点
    double goal_x = 0.0;
    double goal_y = 0.0;
    double goal_yaw = 0.0;
    bool goal_pose_received = false;
};

Q_DECLARE_METATYPE(RenderData)
