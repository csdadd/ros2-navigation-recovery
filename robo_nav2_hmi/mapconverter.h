#ifndef MAPCONVERTER_H
#define MAPCONVERTER_H

#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QTransform>
#include <nav_msgs/msg/occupancy_grid.hpp>

class MapConverter
{
public:
    static QImage convertToImage(const nav_msgs::msg::OccupancyGrid::SharedPtr& map);

    static QPointF mapToImage(double mapX, double mapY,
                              double resolution, double originX, double originY);

    static QPointF imageToMap(int imageX, int imageY,
                              double resolution, double originX, double originY);

    static QPolygonF createRobotPolygon(double x, double y, double yaw, double size);
};

#endif
