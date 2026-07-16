#ifndef MAPLOADER_H
#define MAPLOADER_H

#include <QString>
#include <QMap>
#include <QVariant>
#include <QImage>
#include <nav_msgs/msg/occupancy_grid.hpp>

class MapLoader
{
public:
    static nav_msgs::msg::OccupancyGrid::SharedPtr loadFromFile(const QString& yamlPath);

    static QMap<QString, QVariant> parseYaml(const QString& yamlPath);

    static QImage loadPgm(const QString& pgmPath);

private:
    static QVariant parseYamlValue(const QString& line);
    static QList<QVariant> parseYamlList(const QString& line);
};

#endif
