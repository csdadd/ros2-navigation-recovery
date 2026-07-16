#include "historylogmodel.h"
#include "logutils.h"
#include <QColor>

HistoryLogTableModel::HistoryLogTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

HistoryLogTableModel::~HistoryLogTableModel() = default;

int HistoryLogTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_logs.size();
}

int HistoryLogTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 4;
}

QVariant HistoryLogTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_logs.size() || index.column() >= 4) {
        return QVariant();
    }

    const StorageLogEntry& entry = m_logs[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
        case 1:
            return LogUtils::levelToString(entry.level);
        case 2:
            return entry.source.isEmpty() ? "系统" : entry.source;
        case 3:
            return entry.message;
        default:
            return QVariant();
        }
    }

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
        default:
            return QColor(0, 0, 0);
        }
    }

    return QVariant();
}

QVariant HistoryLogTableModel::headerData(int section, Qt::Orientation orientation, int role) const
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

void HistoryLogTableModel::setQueryResults(const QVector<StorageLogEntry>& entries)
{
    beginResetModel();
    m_logs = entries;
    endResetModel();
}

void HistoryLogTableModel::clear()
{
    if (m_logs.isEmpty()) {
        return;
    }

    beginResetModel();
    m_logs.clear();
    m_totalCount = 0;
    m_currentPage = 1;
    endResetModel();
}

StorageLogEntry HistoryLogTableModel::getLogEntry(int row) const
{
    if (row < 0 || row >= m_logs.size()) {
        return StorageLogEntry();
    }
    return m_logs[row];
}

int HistoryLogTableModel::getTotalCount() const
{
    return m_totalCount;
}

void HistoryLogTableModel::setPaginationInfo(int totalCount, int currentPage, int pageSize)
{
    m_totalCount = totalCount;
    m_currentPage = currentPage;
    m_pageSize = pageSize;
}

int HistoryLogTableModel::getCurrentPage() const
{
    return m_currentPage;
}

int HistoryLogTableModel::getPageSize() const
{
    return m_pageSize;
}

int HistoryLogTableModel::getTotalPages() const
{
    if (m_pageSize <= 0) {
        return 0;
    }
    return (m_totalCount + m_pageSize - 1) / m_pageSize;
}
