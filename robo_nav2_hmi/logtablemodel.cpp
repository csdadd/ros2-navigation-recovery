#include "logtablemodel.h"
#include "logutils.h"
#include <QColor>
#include <QDebug>
#include <algorithm>

LogTableModel::LogTableModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_maxLogs(1000)
    , m_storageEngine(nullptr)
    , m_batchUpdateTimer(new QTimer(this))
    , m_batchUpdateEnabled(false)
    , m_batchUpdateInterval(200)
{
    connect(m_batchUpdateTimer, &QTimer::timeout, this, &LogTableModel::onBatchUpdateTimer);
}

LogTableModel::~LogTableModel()
{
    if (m_batchUpdateTimer->isActive()) {
        m_batchUpdateTimer->stop();
    }
    flushPendingLogs();
}

int LogTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_logs.size();
}

int LogTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 4;
}

QVariant LogTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_logs.size() || index.column() >= 4) {
        return QVariant();
    }

    const LogEntry& entry = m_logs[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
        case 1:
            return LogUtils::levelToString(entry.level);
        case 2:
            return entry.source;
        case 3:
            return entry.message;
        default:
            return QVariant();
        }
    }

    // 自定义角色：返回整数类型的日志级别（用于过滤）
    if (role == LevelRole && index.column() == 1) {
        return static_cast<int>(entry.level);
    }

    if (role == Qt::TextAlignmentRole) {
        if (index.column() == 1) {
            return int(Qt::AlignCenter);
        }
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole) {
        const LogEntry& entry = m_logs[index.row()];
        switch (entry.level) {
        case LogLevel::DEBUG:
            return QColor(128, 128, 128);
        case LogLevel::INFO:
            return QColor(0, 0, 0);
        case LogLevel::WARNING:
            return QColor(255, 165, 0);
        case LogLevel::ERROR:
            return QColor(255, 0, 0);
        case LogLevel::FATAL:
            return QColor(139, 0, 0);
        case LogLevel::HIGHFREQ:
            return QColor(100, 100, 255);  // 蓝色表示高频日志
        default:
            return QColor(0, 0, 0);
        }
    }

    return QVariant();
}

QVariant LogTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return "时间";
        case 1:
            return "级别";
        case 2:
            return "来源";
        case 3:
            return "消息";
        default:
            return QVariant();
        }
    }

    return QVariant();
}

void LogTableModel::addLogEntry(const LogEntry& entry)
{
    m_pendingLogs.append(entry);

    // 如果批量更新未启用或待处理日志超过阈值，立即刷新
    if (!m_batchUpdateEnabled || m_pendingLogs.size() >= BATCH_THRESHOLD) {
        flushPendingLogs();
    }
}

void LogTableModel::addLogEntries(const QVector<LogEntry>& entries)
{
    if (entries.isEmpty()) {
        return;
    }

    beginInsertRows(QModelIndex(), m_logs.size(), m_logs.size() + entries.size() - 1);
    m_logs += entries;
    endInsertRows();

    enforceMaxLogs();
}

void LogTableModel::clearLogs()
{
    if (m_logs.isEmpty()) {
        return;
    }

    beginResetModel();
    m_logs.clear();
    endResetModel();
}

LogEntry LogTableModel::getLogEntry(int row) const
{
    if (row < 0 || row >= m_logs.size()) {
        return LogEntry();
    }
    return m_logs[row];
}

void LogTableModel::setMaxLogs(int maxLogs)
{
    m_maxLogs = maxLogs;
    enforceMaxLogs();
}

int LogTableModel::getMaxLogs() const
{
    return m_maxLogs;
}

void LogTableModel::enforceMaxLogs()
{
    if (m_logs.size() > m_maxLogs) {
        int removeCount = m_logs.size() - m_maxLogs;
        beginRemoveRows(QModelIndex(), 0, removeCount - 1);
        m_logs.erase(m_logs.begin(), m_logs.begin() + removeCount);
        endRemoveRows();
    }
}

void LogTableModel::setStorageEngine(LogStorageEngine* engine)
{
    m_storageEngine = engine;
}

LogEntry LogTableModel::convertStorageEntry(const StorageLogEntry& storageEntry) const
{
    LogEntry entry;
    entry.message = storageEntry.message;
    entry.level = storageEntry.level;
    entry.timestamp = storageEntry.timestamp;
    entry.source = storageEntry.source;
    return entry;
}

void LogTableModel::setBatchUpdateEnabled(bool enabled)
{
    m_batchUpdateEnabled = enabled;
    if (enabled) {
        m_batchUpdateTimer->start(m_batchUpdateInterval);
    } else {
        m_batchUpdateTimer->stop();
        flushPendingLogs();
    }
}

void LogTableModel::setBatchUpdateInterval(int ms)
{
    m_batchUpdateInterval = ms;
    if (m_batchUpdateTimer->isActive()) {
        m_batchUpdateTimer->setInterval(m_batchUpdateInterval);
    }
}

void LogTableModel::flushPendingLogs()
{
    if (m_pendingLogs.isEmpty()) {
        return;
    }

    addLogEntries(m_pendingLogs);
    m_pendingLogs.clear();
}

void LogTableModel::onBatchUpdateTimer()
{
    flushPendingLogs();
}
