#include "mapmarker.h"
#include <QDebug>

MapMarkerManager::MapMarkerManager()
{
}

void MapMarkerManager::addMarker(const MapMarker& marker)
{
    if (marker.name.isEmpty()) {
        qWarning() << "[MapMarkerManager] 添加标记点失败 - 名称为空";
        return;
    }

    if (contains(marker.name)) {
        qWarning() << "[MapMarkerManager] 添加标记点失败 - 标记点已存在:" << marker.name;
        return;
    }

    m_markers.append(marker);
    qDebug() << "[MapMarkerManager] 添加标记点成功 - 名称:" << marker.name << "位置:(" << marker.position.x() << "," << marker.position.y() << ")";
}

void MapMarkerManager::removeMarker(const QString& name)
{
    for (int i = 0; i < m_markers.size(); ++i) {
        if (m_markers[i].name == name) {
            m_markers.removeAt(i);
            qDebug() << "[MapMarkerManager] 删除标记点成功 - 名称:" << name;
            return;
        }
    }

    qWarning() << "[MapMarkerManager] 删除标记点失败 - 未找到标记点:" << name;
}

QList<MapMarker> MapMarkerManager::getMarkers() const
{
    return m_markers;
}

void MapMarkerManager::clear()
{
    m_markers.clear();
    qDebug() << "[MapMarkerManager] 清除所有标记点";
}

bool MapMarkerManager::contains(const QString& name) const
{
    for (const MapMarker& marker : m_markers) {
        if (marker.name == name) {
            return true;
        }
    }
    return false;
}

MapMarker MapMarkerManager::getMarker(const QString& name) const
{
    for (const MapMarker& marker : m_markers) {
        if (marker.name == name) {
            return marker;
        }
    }

    qWarning() << "[MapMarkerManager] 获取标记点失败 - 未找到标记点:" << name;
    return MapMarker();
}
