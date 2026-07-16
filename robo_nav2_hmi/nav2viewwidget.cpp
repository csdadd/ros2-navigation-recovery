#include "nav2viewwidget.h"
#include "geometryutils.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <cmath>

Nav2ViewWidget::Nav2ViewWidget(QWidget* parent)
    : QWidget(parent)
    , map_loaded_(false)
    , coord_transformer_(0.05, 0.0, 0.0, 0)
    , buffer_swap_pending_(false)
    , robot_length_(0.5)
    , robot_width_(0.4)
    , mouse_dragging_(false)
{
    // 初始化刷新定时器，限制刷新频率为25Hz（40ms）
    update_timer_ = new QTimer(this);
    update_timer_->setInterval(40);
    connect(update_timer_, &QTimer::timeout, this, [this]() {
        if (update_pending_.exchange(false)) {
            this->update();
        }
    });
    update_timer_->start();
}

void Nav2ViewWidget::setRobotSize(double length, double width) {
    robot_length_ = length;
    robot_width_ = width;
}

void Nav2ViewWidget::onRenderDataReady(const RenderData& data) {
    {
        std::lock_guard<std::mutex> lock(back_buffer_mutex_);
        // 保留本地设置的目标，除非 DataProcessor 确认收到了新的目标
        double saved_goal_x = back_buffer_.goal_x;
        double saved_goal_y = back_buffer_.goal_y;
        double saved_goal_yaw = back_buffer_.goal_yaw;
        bool had_local_goal = back_buffer_.goal_pose_received && !data.goal_pose_received;

        back_buffer_ = data;

        if (had_local_goal) {
            // 恢复本地设置的目标信息
            back_buffer_.goal_x = saved_goal_x;
            back_buffer_.goal_y = saved_goal_y;
            back_buffer_.goal_yaw = saved_goal_yaw;
            back_buffer_.goal_pose_received = true;
        }
    }
    buffer_swap_pending_ = true;
    update_pending_ = true;
}

void Nav2ViewWidget::onMapLoaded(const QImage& map_image, const CoordinateTransformer& transformer) {
    map_image_ = map_image;
    coord_transformer_ = transformer;
    map_loaded_ = true;

    qDebug() << "[Nav2ViewWidget] 地图已加载，尺寸:" << map_image_.width() << "x" << map_image_.height();
    update();
}

double Nav2ViewWidget::getRobotX() const {
    return front_buffer_.robot_x;
}

double Nav2ViewWidget::getRobotY() const {
    return front_buffer_.robot_y;
}

void Nav2ViewWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    // 交换双缓冲（如果有新数据）
    if (buffer_swap_pending_.exchange(false)) {
        std::lock_guard<std::mutex> lock(back_buffer_mutex_);
        front_buffer_ = back_buffer_;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    ViewTransform vt{1.0, 0.0, 0.0};

    if (map_loaded_) {
        vt = calculateViewTransform(width(), height(), map_image_.width(), map_image_.height());

        // 使用缓存的缩放地图（仅在必要时重新计算）
        QSize current_size(width(), height());
        if (cached_scale_ != vt.scale || cached_widget_size_ != current_size || cached_scaled_map_.isNull()) {
            cached_scaled_map_ = map_image_.scaled(
                map_image_.width() * vt.scale,
                map_image_.height() * vt.scale,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );
            cached_scale_ = vt.scale;
            cached_widget_size_ = current_size;
        }
        painter.drawImage(QPointF(vt.offset_x, vt.offset_y), cached_scaled_map_);
    }

    if (!front_buffer_.path_points.empty()) {
        painter.setPen(QPen(QColor(0, 0, 255), 2));
        for (size_t i = 1; i < front_buffer_.path_points.size(); ++i) {
            QPointF p1 = front_buffer_.path_points[i - 1] * vt.scale + QPointF(vt.offset_x, vt.offset_y);
            QPointF p2 = front_buffer_.path_points[i] * vt.scale + QPointF(vt.offset_x, vt.offset_y);
            painter.drawLine(p1, p2);
        }
    }

    // 绘制局部路径（绿色实线，线宽3）
    if (!front_buffer_.local_path_points.empty()) {
        painter.setPen(QPen(QColor(0, 255, 0), 3));
        for (size_t i = 1; i < front_buffer_.local_path_points.size(); ++i) {
            QPointF p1 = front_buffer_.local_path_points[i - 1] * vt.scale + QPointF(vt.offset_x, vt.offset_y);
            QPointF p2 = front_buffer_.local_path_points[i] * vt.scale + QPointF(vt.offset_x, vt.offset_y);
            painter.drawLine(p1, p2);
        }
    }

    // 绘制激光点云（红色小点，点大小2）
    if (!front_buffer_.scan_points.empty()) {
        painter.setPen(QPen(QColor(255, 0, 0), 2));
        for (const auto& pt : front_buffer_.scan_points) {
            QPointF p = pt * vt.scale + QPointF(vt.offset_x, vt.offset_y);
            painter.drawPoint(p);
        }
    }

    if (front_buffer_.robot_pose_received) {
        QPointF center = coord_transformer_.mapToQt(front_buffer_.robot_x, front_buffer_.robot_y);
        center = center * vt.scale + QPointF(vt.offset_x, vt.offset_y);
        // 使用固定像素尺寸显示机器人，避免在高分辨率地图下显示过小
        double pixel_length = 30.0;  // 固定显示尺寸为30像素
        double pixel_width = 24.0;   // 根据长宽比调整

        QTransform transform;
        transform.translate(center.x(), center.y());
        transform.rotate(-front_buffer_.robot_yaw * 180.0 / M_PI);

        QRectF rect(-pixel_length / 2, -pixel_width / 2, pixel_length, pixel_width);
        QPolygonF robot_rect = transform.map(rect);
        painter.setBrush(QColor(0, 200, 0));
        painter.setPen(Qt::black);
        painter.drawPolygon(robot_rect);

        QPolygonF triangle;
        // 使用固定箭头尺寸，避免在高分辨率地图下显示过小
        double arrow_size = 10.0;  // 固定箭头尺寸为10像素
        triangle << QPointF(pixel_length / 2, 0)
                 << QPointF(pixel_length / 2 - arrow_size, -arrow_size / 2)
                 << QPointF(pixel_length / 2 - arrow_size, arrow_size / 2);
        QPolygonF triangle_transformed = transform.map(triangle);
        painter.drawPolygon(triangle_transformed);
    }

    if (front_buffer_.goal_pose_received) {
        QPointF center = coord_transformer_.mapToQt(front_buffer_.goal_x, front_buffer_.goal_y);
        center = center * vt.scale + QPointF(vt.offset_x, vt.offset_y);
        // 使用固定箭头尺寸，避免在高分辨率地图下显示过小
        double arrow_size = 25.0;  // 固定目标箭头尺寸为25像素

        QTransform transform;
        transform.translate(center.x(), center.y());
        transform.rotate(-front_buffer_.goal_yaw * 180.0 / M_PI);

        QPolygonF arrow;
        arrow << QPointF(arrow_size / 2, 0)
              << QPointF(-arrow_size / 2, -arrow_size / 3)
              << QPointF(-arrow_size / 2, arrow_size / 3);

        QPolygonF arrow_transformed = transform.map(arrow);
        painter.setBrush(QColor(0, 0, 255));
        painter.setPen(Qt::black);
        painter.drawPolygon(arrow_transformed);
    }

    if (mouse_dragging_) {
        double drag_yaw = std::atan2(
            mouse_press_pos_.y() - mouse_current_pos_.y(),
            mouse_current_pos_.x() - mouse_press_pos_.x()
        );
        // 使用固定箭头尺寸，与目标箭头保持一致
        double arrow_size = 25.0;  // 固定拖拽预览箭头尺寸为25像素

        QTransform transform;
        transform.translate(mouse_press_pos_.x(), mouse_press_pos_.y());
        transform.rotate(-drag_yaw * 180.0 / M_PI);

        QPolygonF arrow;
        arrow << QPointF(arrow_size / 2, 0)
              << QPointF(-arrow_size / 2, -arrow_size / 3)
              << QPointF(-arrow_size / 2, arrow_size / 3);

        QPolygonF arrow_transformed = transform.map(arrow);
        painter.setBrush(QColor(0, 0, 255, 150));
        painter.setPen(Qt::black);
        painter.drawPolygon(arrow_transformed);

        painter.setPen(QPen(QColor(0, 0, 255, 150), 1, Qt::DashLine));
        painter.drawLine(mouse_press_pos_, mouse_current_pos_);
    }
}

void Nav2ViewWidget::mousePressEvent(QMouseEvent* event) {
    // 权限检查
    if (!m_canOperate) {
        return;  // VIEWER 权限禁止交互
    }

    if (event->button() == Qt::LeftButton) {
        mouse_dragging_ = true;
        mouse_press_pos_ = event->pos();
        mouse_current_pos_ = event->pos();
        update();
    }
}

void Nav2ViewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_canOperate) {
        return;
    }

    if (mouse_dragging_) {
        mouse_current_pos_ = event->pos();
        update();
    }
}

void Nav2ViewWidget::mouseReleaseEvent(QMouseEvent* event) {
    // 权限检查
    if (!m_canOperate) {
        return;  // VIEWER 权限禁止设置目标点
    }

    if (event->button() == Qt::LeftButton && mouse_dragging_) {
        mouse_dragging_ = false;

        if (!map_loaded_) {
            return;
        }

        ViewTransform vt = calculateViewTransform(width(), height(), map_image_.width(), map_image_.height());

        // 将屏幕坐标转换为地图坐标
        QPointF adjusted_pos = (mouse_press_pos_ - QPointF(vt.offset_x, vt.offset_y)) / vt.scale;
        double x, y;
        coord_transformer_.qtToMap(adjusted_pos, x, y);

        double yaw = std::atan2(
            mouse_press_pos_.y() - mouse_current_pos_.y(),
            mouse_current_pos_.x() - mouse_press_pos_.x()
        );

        // 存储目标到两个缓冲区
        {
            std::lock_guard<std::mutex> lock(back_buffer_mutex_);
            back_buffer_.goal_x = x;
            back_buffer_.goal_y = y;
            back_buffer_.goal_yaw = yaw;
            back_buffer_.goal_pose_received = true;
        }
        front_buffer_.goal_x = x;
        front_buffer_.goal_y = y;
        front_buffer_.goal_yaw = yaw;
        front_buffer_.goal_pose_received = true;

        // 发射预览信号，用于更新 UI 显示
        Q_EMIT goalPosePreview(x, y, yaw);
        update();
    }
}

void Nav2ViewWidget::clearGoal()
{
    // 同步两个缓冲区
    {
        std::lock_guard<std::mutex> lock(back_buffer_mutex_);
        back_buffer_.goal_x = 0.0;
        back_buffer_.goal_y = 0.0;
        back_buffer_.goal_yaw = 0.0;
        back_buffer_.goal_pose_received = false;
    }
    front_buffer_.goal_x = 0.0;
    front_buffer_.goal_y = 0.0;
    front_buffer_.goal_yaw = 0.0;
    front_buffer_.goal_pose_received = false;

    Q_EMIT goalCleared();
}

void Nav2ViewWidget::setOperatePermission(bool canOperate)
{
    m_canOperate = canOperate;
    qDebug() << "[Nav2ViewWidget] 操作权限已设置为:" << canOperate;
}
