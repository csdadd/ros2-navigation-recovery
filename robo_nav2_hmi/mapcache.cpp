#include "mapcache.h"
#include <QDebug>

MapCache::MapCache(int maxSize, QObject* parent)
    : QObject(parent)
    , m_maxSize(maxSize)
{
}

void MapCache::add(const QString& key, const nav_msgs::msg::OccupancyGrid::SharedPtr& map)
{
    QWriteLocker locker(&m_lock);

    if (!map) {
        qWarning() << "[MapCache] 尝试添加空地图到缓存";
        return;
    }

    auto it = m_index.find(key);

    // 键已存在：更新数据并移到链表头部
    if (it != m_index.end()) {
        it.value()->second = map;  // 更新地图数据
        m_lruList.splice(m_lruList.begin(), m_lruList, it.value());  // 移到头部 O(1)
        qDebug() << "[MapCache] 更新缓存:" << key;
        return;
    }

    // 键不存在且缓存已满：先淘汰
    if (m_lruList.size() >= static_cast<size_t>(m_maxSize)) {
        locker.unlock();
        evictOldest();
        locker.relock();
    }

    // 插入新项到链表头部
    m_lruList.emplace_front(key, map);
    m_index[key] = m_lruList.begin();
    qDebug() << "[MapCache] 添加地图到缓存:" << key << "当前缓存大小:" << m_lruList.size();
}

nav_msgs::msg::OccupancyGrid::SharedPtr MapCache::get(const QString& key)
{
    QWriteLocker locker(&m_lock);

    auto it = m_index.find(key);

    if (it != m_index.end()) {
        // 找到：移到链表头部（splice O(1)）
        m_lruList.splice(m_lruList.begin(), m_lruList, it.value());
        qDebug() << "[MapCache] 从缓存获取地图:" << key;
        return it.value()->second;
    }

    qWarning() << "[MapCache] 缓存中未找到地图:" << key;
    return nullptr;
}

void MapCache::remove(const QString& key)
{
    QWriteLocker locker(&m_lock);

    auto it = m_index.find(key);

    if (it != m_index.end()) {
        m_lruList.erase(it.value());  // 从链表删除
        m_index.erase(it);            // 从索引删除
        qDebug() << "[MapCache] 从缓存移除地图:" << key;
    } else {
        qWarning() << "[MapCache] 尝试移除不存在的地图:" << key;
    }
}

void MapCache::clear()
{
    QWriteLocker locker(&m_lock);

    int count = static_cast<int>(m_lruList.size());
    m_lruList.clear();
    m_index.clear();
    qDebug() << "[MapCache] 清空缓存，移除了" << count << "个地图";
}

bool MapCache::contains(const QString& key) const
{
    QReadLocker locker(&m_lock);
    return m_index.contains(key);  // O(1) 查找
}

int MapCache::size() const
{
    QReadLocker locker(&m_lock);
    return static_cast<int>(m_lruList.size());
}

int MapCache::maxSize() const
{
    QReadLocker locker(&m_lock);
    return m_maxSize;
}

void MapCache::setMaxSize(int maxSize)
{
    QWriteLocker locker(&m_lock);

    if (maxSize <= 0) {
        qWarning() << "[MapCache] 无效的缓存大小:" << maxSize;
        return;
    }

    m_maxSize = maxSize;

    while (m_lruList.size() > static_cast<size_t>(m_maxSize)) {
        locker.unlock();
        evictOldest();
        locker.relock();
    }

    qDebug() << "[MapCache] 设置缓存大小为:" << m_maxSize;
}

void MapCache::evictOldest()
{
    QWriteLocker locker(&m_lock);

    if (m_lruList.empty()) {
        return;
    }

    // 删除链表尾部（最旧项）- O(1)
    QString key = m_lruList.back().first;  // 复制值，避免引用失效
    m_lruList.pop_back();
    m_index.remove(key);

    qDebug() << "[MapCache] 淘汰最旧的地图:" << key;
}
