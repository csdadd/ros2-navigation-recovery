#ifndef MAPCACHE_H
#define MAPCACHE_H

#include <QString>
#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <list>
#include <utility>

class MapCache : public QObject
{
    Q_OBJECT

public:
    explicit MapCache(int maxSize = 10, QObject* parent = nullptr);

    void add(const QString& key, const nav_msgs::msg::OccupancyGrid::SharedPtr& map);

    nav_msgs::msg::OccupancyGrid::SharedPtr get(const QString& key);

    void remove(const QString& key);

    void clear();

    bool contains(const QString& key) const;

    int size() const;

    int maxSize() const;

    void setMaxSize(int maxSize);

private:
    void evictOldest();

    // LRU缓存数据类型定义
    using CacheItem = std::pair<QString, nav_msgs::msg::OccupancyGrid::SharedPtr>;
    using CacheList = std::list<CacheItem>;
    using CacheIndex = QHash<QString, CacheList::iterator>;

    CacheList m_lruList;    // LRU链表：前面是最近访问，后面是最久未访问
    CacheIndex m_index;     // 哈希索引：键 -> 链表迭代器

    mutable QReadWriteLock m_lock;
    int m_maxSize;
};

#endif
