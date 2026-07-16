#ifndef MAPMARKER_H
#define MAPMARKER_H

#include <QString>
#include <QPointF>
#include <QColor>
#include <QList>

struct MapMarker
{
    QString name;
    QPointF position;
    QColor color;
    QString description;

    MapMarker() = default;

    MapMarker(double x, double y, const QColor& c, const QString& n, const QString& desc)
        : name(n), position(x, y), color(c), description(desc) {}
};

class MapMarkerManager
{
public:
    MapMarkerManager();

    void addMarker(const MapMarker& marker);
    void removeMarker(const QString& name);
    QList<MapMarker> getMarkers() const;
    void clear();
    bool contains(const QString& name) const;
    MapMarker getMarker(const QString& name) const;

private:
    QList<MapMarker> m_markers;
};

#endif
