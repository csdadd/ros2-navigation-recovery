#include "pathvisualizer.h"
#include "mapconverter.h"
#include <QDebug>

PathVisualizer::PathVisualizer(QGraphicsScene* scene, QObject* parent)
    : QObject(parent)
    , m_scene(scene)
    , m_pathItem(nullptr)
    , m_pathColor(Qt::green)
    , m_pathWidth(2.0)
    , m_pathStyle(Qt::SolidLine)
{
    qDebug() << "[PathVisualizer] 构造函数";

    if (!m_scene) {
        qCritical() << "[PathVisualizer] 错误：场景指针为空";
        return;
    }

    m_pathItem = new QGraphicsPathItem();
    m_pathItem->setPen(QPen(m_pathColor, m_pathWidth, m_pathStyle));
    m_pathItem->setZValue(1);
    m_scene->addItem(m_pathItem);

    qDebug() << "[PathVisualizer] 初始化成功 - 颜色:" << m_pathColor.name() << "宽度:" << m_pathWidth;
}

PathVisualizer::~PathVisualizer()
{
    qDebug() << "[PathVisualizer] 析构函数";

    if (m_scene && m_pathItem) {
        m_scene->removeItem(m_pathItem);
        delete m_pathItem;
        m_pathItem = nullptr;
    }
}

void PathVisualizer::updatePath(const nav_msgs::msg::Path& path, double resolution, const QPointF& origin)
{
    if (path.poses.empty()) {
        qDebug() << "[PathVisualizer] 路径为空，清除显示";
        clearPath();
        return;
    }

    QPainterPath painterPath;
    QPointF firstPoint = mapToScene(path.poses[0].pose.position.x,
                                    path.poses[0].pose.position.y,
                                    resolution, origin);
    painterPath.moveTo(firstPoint);

    for (size_t i = 1; i < path.poses.size(); ++i) {
        QPointF point = mapToScene(path.poses[i].pose.position.x,
                                   path.poses[i].pose.position.y,
                                   resolution, origin);
        painterPath.lineTo(point);
    }

    m_pathItem->setPath(painterPath);
    m_pathItem->setVisible(true);

    qDebug() << "[PathVisualizer] 路径更新成功 - 路径点数:" << path.poses.size() << "分辨率:" << resolution;
}

void PathVisualizer::clearPath()
{
    m_pathItem->setPath(QPainterPath());
    m_pathItem->setVisible(false);
    qDebug() << "[PathVisualizer] 路径已清除";
}

void PathVisualizer::setPathColor(const QColor& color)
{
    m_pathColor = color;
    m_pathItem->setPen(QPen(m_pathColor, m_pathWidth, m_pathStyle));
    qDebug() << "[PathVisualizer] 路径颜色已更改:" << color.name();
}

void PathVisualizer::setPathWidth(double width)
{
    m_pathWidth = width;
    m_pathItem->setPen(QPen(m_pathColor, m_pathWidth, m_pathStyle));
    qDebug() << "[PathVisualizer] 路径宽度已更改:" << width;
}

void PathVisualizer::setPathStyle(Qt::PenStyle style)
{
    m_pathStyle = style;
    m_pathItem->setPen(QPen(m_pathColor, m_pathWidth, m_pathStyle));
    qDebug() << "[PathVisualizer] 路径样式已更改:" << style;
}

QPointF PathVisualizer::mapToScene(double x, double y, double resolution, const QPointF& origin)
{
    return MapConverter::mapToImage(x, y, resolution, origin.x(), origin.y());
}
