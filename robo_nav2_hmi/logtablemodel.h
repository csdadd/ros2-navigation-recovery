#ifndef LOGTABLEMODEL_H
#define LOGTABLEMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QDateTime>
#include <QTimer>
#include "logthread.h"
#include "logstorageengine.h"

class LogTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    // 自定义角色，用于获取原始日志级别（整数）
    enum LogRoles {
        LevelRole = Qt::UserRole + 1
    };

    explicit LogTableModel(QObject* parent = nullptr);
    ~LogTableModel();

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void addLogEntry(const LogEntry& entry);
    void addLogEntries(const QVector<LogEntry>& entries);
    void clearLogs();
    LogEntry getLogEntry(int row) const;

    void setMaxLogs(int maxLogs);
    int getMaxLogs() const;

    void setStorageEngine(LogStorageEngine* engine);
    
    // 批量更新控制
    void setBatchUpdateEnabled(bool enabled);
    void setBatchUpdateInterval(int ms);
    void flushPendingLogs();

private slots:
    void onBatchUpdateTimer();

private:
    void enforceMaxLogs();
    LogEntry convertStorageEntry(const StorageLogEntry& storageEntry) const;

private:
    QVector<LogEntry> m_logs;
    int m_maxLogs;
    LogStorageEngine* m_storageEngine;

    // 批量更新相关
    QTimer* m_batchUpdateTimer;
    QVector<LogEntry> m_pendingLogs;
    bool m_batchUpdateEnabled;
    int m_batchUpdateInterval;
    static const int BATCH_THRESHOLD = 100;
};

#endif // LOGTABLEMODEL_H
