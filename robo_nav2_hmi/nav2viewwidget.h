#pragma once

#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QTimer>
#include <atomic>
#include <vector>
#include <memory>
#include <mutex>
#include "coordinatetransformer.h"
#include "geometryutils.h"
#include "renderdata.h"

class Nav2ViewWidget : public QWidget {
    Q_OBJECT

signals:
    void dataUpdated();
    void goalPosePreview(double x, double y, double yaw);
    void goalCleared();

public slots:
    void onRenderDataReady(const RenderData& data);
    void onMapLoaded(const QImage& map_image, const CoordinateTransformer& transformer);

public:
    explicit Nav2ViewWidget(QWidget* parent = nullptr);

    void setRobotSize(double length, double width);
    void clearGoal();

    // 获取当前机器人位置（地图坐标系）
    double getRobotX() const;
    double getRobotY() const;

    // 设置操作权限
    void setOperatePermission(bool canOperate);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QImage map_image_;
    bool map_loaded_;

    CoordinateTransformer coord_transformer_{0.05, 0.0, 0.0, 0};  // 坐标转换器（用于鼠标交互，初始值）

    // 双缓冲
    RenderData back_buffer_;
    RenderData front_buffer_;
    std::mutex back_buffer_mutex_;
    std::atomic<bool> buffer_swap_pending_;

    // 机器人尺寸
    double robot_length_;
    double robot_width_;

    // 鼠标交互
    bool mouse_dragging_;
    QPointF mouse_press_pos_;
    QPointF mouse_current_pos_;

    // 操作权限标志
    bool m_canOperate = true;  // 默认允许操作（向后兼容）

    // 缓存缩放后的地图图像（用于优化paintEvent性能）
    mutable QImage cached_scaled_map_;
    mutable double cached_scale_ = -1.0;
    mutable QSize cached_widget_size_;

    // 刷新频率控制
    QTimer* update_timer_ = nullptr;
    std::atomic<bool> update_pending_{false};
};
