#ifndef PATHVISUALIZER_H
#define PATHVISUALIZER_H

#include <QObject>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <nav_msgs/msg/path.hpp>
#include <memory>

class PathVisualizer : public QObject
{
    Q_OBJECT

public:
    explicit PathVisualizer(QGraphicsScene* scene, QObject* parent = nullptr);
    ~PathVisualizer();

    void updatePath(const nav_msgs::msg::Path& path, double resolution, const QPointF& origin);
    void clearPath();
    void setPathColor(const QColor& color);
    void setPathWidth(double width);
    void setPathStyle(Qt::PenStyle style);

private:
    QPointF mapToScene(double x, double y, double resolution, const QPointF& origin);

private:
    QGraphicsScene* m_scene;
    QGraphicsPathItem* m_pathItem;
    QColor m_pathColor;
    double m_pathWidth;
    Qt::PenStyle m_pathStyle;
};

#endif
