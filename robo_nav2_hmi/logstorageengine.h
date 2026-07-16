#ifndef LOGSTORAGEENGINE_H
#define LOGSTORAGEENGINE_H

#include "loglevel.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVector>
#include <QReadWriteLock>
#include <QSqlDatabase>
#include <QSqlError>
#include <QTimer>
#include <QFileInfo>

struct StorageLogEntry {
    QString message;
    LogLevel level;
    QDateTime timestamp;
    QString source;
    QString filePath;
    int lineNumber;

    StorageLogEntry() : level(LogLevel::INFO), lineNumber(0) {}
    StorageLogEntry(const QString& msg, LogLevel lvl, const QDateTime& ts,
                     const QString& src = "", const QString& file = "", int line = 0)
        : message(msg), level(lvl), timestamp(ts), source(src),
          filePath(file), lineNumber(line) {}
};

Q_DECLARE_METATYPE(StorageLogEntry)
Q_DECLARE_METATYPE(QVector<StorageLogEntry>)

// 日志保留策略配置
struct RetentionPolicy {
    int retentionDays = 30;         // 普通日志保留天数
    int highFreqRetentionDays = 10; // 高频日志保留天数
    int odometryRetentionDays = 3;  // 里程计保留天数
    qint64 maxDbSizeMB = 500;       // 数据库最大大小（MB）
    int cleanupIntervalHours = 24;  // 定时清理间隔（小时）

    RetentionPolicy() = default;
};

class LogStorageEngine : public QObject
{
    Q_OBJECT

public:
    explicit LogStorageEngine(QObject* parent = nullptr);
    ~LogStorageEngine();

    bool initialize(const QString& dbPath = QString());
    bool isInitialized() const;

    bool insertLog(const StorageLogEntry& entry);
    bool insertLogs(const QVector<StorageLogEntry>& entries);

    int getLogCount(const QDateTime& startTime = QDateTime(),
                    const QDateTime& endTime = QDateTime(),
                    LogLevel minLevel = LogLevel::DEBUG);

    bool clearLogs(const QDateTime& beforeTime = QDateTime());
    bool vacuum();

    QString getLastError() const;
    QString getDbPath() const;

    // 高频日志相关方法
    bool insertHighFreqLog(const StorageLogEntry& entry);
    bool insertHighFreqLogs(const QVector<StorageLogEntry>& entries);

    QVector<StorageLogEntry> queryHighFreqLogs(const QDateTime& startTime,
                                                const QDateTime& endTime,
                                                int limit = -1,
                                                int offset = 0);
    int getHighFreqLogCount(const QDateTime& startTime = QDateTime(),
                            const QDateTime& endTime = QDateTime());

    bool clearHighFreqLogs(const QDateTime& beforeTime = QDateTime());

    // 里程计日志相关方法
    bool insertOdometryLogs(const QVector<StorageLogEntry>& entries);
    bool clearOdometryLogs(const QDateTime& beforeTime = QDateTime());

    // 自动清理相关方法
    void setRetentionPolicy(const RetentionPolicy& policy);
    RetentionPolicy retentionPolicy() const;
    void startAutoCleanup();
    void stopAutoCleanup();
    bool performCleanup();
    qint64 getDatabaseSize() const;
    double getDatabaseSizeMB() const;

signals:
    void errorOccurred(const QString& error);

private:
    bool createTables();
    bool createIndexes();

private:
    QSqlDatabase m_database;
    QString m_connectionName;
    QString m_dbPath;
    bool m_initialized;
    mutable QReadWriteLock m_lock;
    QString m_lastError;
    static const int BATCH_INSERT_SIZE = 100;

    // 自动清理相关成员
    RetentionPolicy m_retentionPolicy;
    QTimer* m_cleanupTimer = nullptr;

    // 内部清理辅助方法
    bool cleanupByRetentionPolicy();
    bool cleanupBySizeLimit();
};

#endif // LOGSTORAGEENGINE_H
