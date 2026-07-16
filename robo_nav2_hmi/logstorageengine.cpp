#include "logstorageengine.h"
#include "logutils.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QMutexLocker>
#include <QThread>

LogStorageEngine::LogStorageEngine(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
{
}

LogStorageEngine::~LogStorageEngine()
{
    stopAutoCleanup();

    if (m_database.isOpen()) {
        m_database.close();
    }
    if (!m_connectionName.isEmpty()) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool LogStorageEngine::initialize(const QString& dbPath)
{
    QWriteLocker locker(&m_lock);

    if (m_initialized) {
        m_lastError = "Database already initialized";
        return true;
    }

    if (dbPath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(dataDir);
        if (!dir.exists()) {
            dir.mkpath(dataDir);
        }
        m_dbPath = dataDir + "/logs.db";
    } else {
        m_dbPath = dbPath;

    // 确保父目录存在
    QDir dbDir = QFileInfo(m_dbPath).absoluteDir();
    if (!dbDir.exists()) {
        dbDir.mkpath(".");
    }
    }

    m_connectionName = QString("LogStorageEngine_%1").arg(
        reinterpret_cast<quintptr>(QThread::currentThreadId())
    );
    m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);

    m_database.setDatabaseName(m_dbPath);

    if (!m_database.open()) {
        m_lastError = QString("Failed to open database: %1").arg(m_database.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    if (!createTables()) {
        m_database.close();
        return false;
    }

    if (!createIndexes()) {
        m_database.close();
        return false;
    }

    m_initialized = true;
    qDebug() << "[LogStorageEngine] Database initialized:" << m_dbPath;
    return true;
}

bool LogStorageEngine::isInitialized() const
{
    QReadLocker locker(&m_lock);
    return m_initialized;
}

bool LogStorageEngine::createTables()
{
    QSqlQuery query(m_database);

    QString createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            level INTEGER NOT NULL,
            message TEXT NOT NULL,
            source TEXT,
            category TEXT,
            file_path TEXT,
            line_number INTEGER,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    if (!query.exec(createTableSQL)) {
        m_lastError = QString("Failed to create logs table: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    QString createHighFreqTableSQL = R"(
        CREATE TABLE IF NOT EXISTS high_freq_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            level INTEGER NOT NULL,
            message TEXT NOT NULL,
            source TEXT,
            category TEXT,
            file_path TEXT,
            line_number INTEGER,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    if (!query.exec(createHighFreqTableSQL)) {
        m_lastError = QString("Failed to create high_freq_logs table: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    QString createOdometryTableSQL = R"(
        CREATE TABLE IF NOT EXISTS odometry_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp INTEGER NOT NULL,
            level INTEGER NOT NULL,
            message TEXT NOT NULL,
            source TEXT,
            category TEXT,
            file_path TEXT,
            line_number INTEGER,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";

    if (!query.exec(createOdometryTableSQL)) {
        m_lastError = QString("Failed to create odometry_logs table: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool LogStorageEngine::createIndexes()
{
    QSqlQuery query(m_database);

    QString createTimestampIndex = "CREATE INDEX IF NOT EXISTS idx_timestamp ON logs(timestamp)";
    if (!query.exec(createTimestampIndex)) {
        m_lastError = QString("Failed to create timestamp index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    QString createLevelIndex = "CREATE INDEX IF NOT EXISTS idx_level ON logs(level)";
    if (!query.exec(createLevelIndex)) {
        m_lastError = QString("Failed to create level index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    QString createLevelTimestampIndex = "CREATE INDEX IF NOT EXISTS idx_level_timestamp ON logs(level, timestamp)";
    if (!query.exec(createLevelTimestampIndex)) {
        m_lastError = QString("Failed to create level_timestamp index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    QString createSourceIndex = "CREATE INDEX IF NOT EXISTS idx_source ON logs(source)";
    if (!query.exec(createSourceIndex)) {
        m_lastError = QString("Failed to create source index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    // 高频日志表索引
    QString createHfTimestampIndex = "CREATE INDEX IF NOT EXISTS idx_hf_timestamp ON high_freq_logs(timestamp)";
    if (!query.exec(createHfTimestampIndex)) {
        m_lastError = QString("Failed to create high_freq timestamp index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    QString createHfSourceIndex = "CREATE INDEX IF NOT EXISTS idx_hf_source ON high_freq_logs(source)";
    if (!query.exec(createHfSourceIndex)) {
        m_lastError = QString("Failed to create high_freq source index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    // 里程计日志表索引
    QString createOdomTimestampIndex = "CREATE INDEX IF NOT EXISTS idx_odom_timestamp ON odometry_logs(timestamp)";
    if (!query.exec(createOdomTimestampIndex)) {
        m_lastError = QString("Failed to create odometry timestamp index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    QString createOdomSourceIndex = "CREATE INDEX IF NOT EXISTS idx_odom_source ON odometry_logs(source)";
    if (!query.exec(createOdomSourceIndex)) {
        m_lastError = QString("Failed to create odometry source index: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool LogStorageEngine::insertLog(const StorageLogEntry& entry)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO logs (timestamp, level, message, source, category, file_path, line_number)
        VALUES (?, ?, ?, ?, '', ?, ?)
    )");

    query.addBindValue(entry.timestamp.toMSecsSinceEpoch());
    query.addBindValue(static_cast<int>(entry.level));
    query.addBindValue(entry.message);
    query.addBindValue(entry.source);
    query.addBindValue(entry.filePath);
    query.addBindValue(entry.lineNumber);

    if (!query.exec()) {
        m_lastError = QString("Failed to insert log: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool LogStorageEngine::insertLogs(const QVector<StorageLogEntry>& entries)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    if (entries.isEmpty()) {
        return true;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO logs (timestamp, level, message, source, category, file_path, line_number)
        VALUES (?, ?, ?, ?, '', ?, ?)
    )");

    if (!m_database.transaction()) {
        m_lastError = QString("Failed to start transaction: %1").arg(m_database.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    int successCount = 0;
    for (const auto& entry : entries) {
        query.addBindValue(entry.timestamp.toMSecsSinceEpoch());
        query.addBindValue(static_cast<int>(entry.level));
        query.addBindValue(entry.message);
        query.addBindValue(entry.source);
        query.addBindValue(entry.filePath);
        query.addBindValue(entry.lineNumber);

        if (query.exec()) {
            successCount++;
        } else {
            qDebug() << "[LogStorageEngine] Failed to insert log:" << query.lastError().text();
        }
    }

    if (!m_database.commit()) {
        m_lastError = QString("Failed to commit transaction: %1").arg(m_database.lastError().text());
        emit errorOccurred(m_lastError);
        m_database.rollback();
        return false;
    }

    return true;
}

int LogStorageEngine::getLogCount(const QDateTime& startTime,
                                    const QDateTime& endTime,
                                    LogLevel minLevel)
{
    QReadLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return 0;
    }

    QString sql = "SELECT COUNT(*) FROM logs WHERE 1=1";
    QVector<QVariant> bindValues;

    if (startTime.isValid()) {
        sql += " AND timestamp >= ?";
        bindValues.append(startTime.toMSecsSinceEpoch());
    }

    if (endTime.isValid()) {
        sql += " AND timestamp <= ?";
        bindValues.append(endTime.toMSecsSinceEpoch());
    }

    if (minLevel != LogLevel::DEBUG) {
        sql += " AND level >= ?";
        bindValues.append(static_cast<int>(minLevel));
    }

    QSqlQuery query(m_database);
    query.prepare(sql);

    for (const auto& value : bindValues) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        m_lastError = QString("Failed to count logs: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return 0;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool LogStorageEngine::clearLogs(const QDateTime& beforeTime)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);

    if (beforeTime.isValid()) {
        query.prepare("DELETE FROM logs WHERE timestamp < ?");
        query.addBindValue(beforeTime.toMSecsSinceEpoch());
    } else {
        query.prepare("DELETE FROM logs");
    }

    if (!query.exec()) {
        m_lastError = QString("Failed to clear logs: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "[LogStorageEngine] Cleared logs, affected rows:" << query.numRowsAffected();
    return true;
}

bool LogStorageEngine::vacuum()
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);

    if (!query.exec("VACUUM")) {
        m_lastError = QString("Failed to vacuum database: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "[LogStorageEngine] Database vacuumed successfully";
    return true;
}

QString LogStorageEngine::getLastError() const
{
    QReadLocker locker(&m_lock);
    return m_lastError;
}

QString LogStorageEngine::getDbPath() const
{
    QReadLocker locker(&m_lock);
    return m_dbPath;
}

bool LogStorageEngine::insertHighFreqLog(const StorageLogEntry& entry)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO high_freq_logs (timestamp, level, message, source, category, file_path, line_number)
        VALUES (?, ?, ?, ?, '', ?, ?)
    )");

    query.addBindValue(entry.timestamp.toMSecsSinceEpoch());
    query.addBindValue(static_cast<int>(entry.level));
    query.addBindValue(entry.message);
    query.addBindValue(entry.source);
    query.addBindValue(entry.filePath);
    query.addBindValue(entry.lineNumber);

    if (!query.exec()) {
        m_lastError = QString("Failed to insert high freq log: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    return true;
}

bool LogStorageEngine::insertHighFreqLogs(const QVector<StorageLogEntry>& entries)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    if (entries.isEmpty()) {
        return true;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO high_freq_logs (timestamp, level, message, source, category, file_path, line_number)
        VALUES (?, ?, ?, ?, '', ?, ?)
    )");

    if (!m_database.transaction()) {
        m_lastError = QString("Failed to start transaction: %1").arg(m_database.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    int successCount = 0;
    for (const auto& entry : entries) {
        query.addBindValue(entry.timestamp.toMSecsSinceEpoch());
        query.addBindValue(static_cast<int>(entry.level));
        query.addBindValue(entry.message);
        query.addBindValue(entry.source);
        query.addBindValue(entry.filePath);
        query.addBindValue(entry.lineNumber);

        if (query.exec()) {
            successCount++;
        } else {
            qDebug() << "[LogStorageEngine] Failed to insert high freq log:" << query.lastError().text();
        }
    }

    if (!m_database.commit()) {
        m_lastError = QString("Failed to commit transaction: %1").arg(m_database.lastError().text());
        emit errorOccurred(m_lastError);
        m_database.rollback();
        return false;
    }

    return true;
}

QVector<StorageLogEntry> LogStorageEngine::queryHighFreqLogs(const QDateTime& startTime,
                                                             const QDateTime& endTime,
                                                             int limit,
                                                             int offset)
{
    QReadLocker locker(&m_lock);

    QVector<StorageLogEntry> results;

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return results;
    }

    QString sql = "SELECT timestamp, level, message, source, file_path, line_number FROM high_freq_logs WHERE 1=1";
    QVector<QVariant> bindValues;

    if (startTime.isValid()) {
        sql += " AND timestamp >= ?";
        bindValues.append(startTime.toMSecsSinceEpoch());
    }

    if (endTime.isValid()) {
        sql += " AND timestamp <= ?";
        bindValues.append(endTime.toMSecsSinceEpoch());
    }

    sql += " ORDER BY timestamp DESC";

    if (limit > 0) {
        sql += " LIMIT ?";
        bindValues.append(limit);
    }

    if (offset > 0) {
        sql += " OFFSET ?";
        bindValues.append(offset);
    }

    QSqlQuery query(m_database);
    query.prepare(sql);

    for (const auto& value : bindValues) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        m_lastError = QString("Failed to query high freq logs: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return results;
    }

    while (query.next()) {
        StorageLogEntry entry;
        entry.timestamp = QDateTime::fromMSecsSinceEpoch(query.value(0).toLongLong());
        entry.level = static_cast<LogLevel>(query.value(1).toInt());
        entry.message = query.value(2).toString();
        entry.source = query.value(3).toString();
        entry.filePath = query.value(4).toString();
        entry.lineNumber = query.value(5).toInt();
        results.append(entry);
    }

    return results;
}

int LogStorageEngine::getHighFreqLogCount(const QDateTime& startTime,
                                          const QDateTime& endTime)
{
    QReadLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return 0;
    }

    QString sql = "SELECT COUNT(*) FROM high_freq_logs WHERE 1=1";
    QVector<QVariant> bindValues;

    if (startTime.isValid()) {
        sql += " AND timestamp >= ?";
        bindValues.append(startTime.toMSecsSinceEpoch());
    }

    if (endTime.isValid()) {
        sql += " AND timestamp <= ?";
        bindValues.append(endTime.toMSecsSinceEpoch());
    }

    QSqlQuery query(m_database);
    query.prepare(sql);

    for (const auto& value : bindValues) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        m_lastError = QString("Failed to count high freq logs: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return 0;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool LogStorageEngine::clearHighFreqLogs(const QDateTime& beforeTime)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);

    if (beforeTime.isValid()) {
        query.prepare("DELETE FROM high_freq_logs WHERE timestamp < ?");
        query.addBindValue(beforeTime.toMSecsSinceEpoch());
    } else {
        query.prepare("DELETE FROM high_freq_logs");
    }

    if (!query.exec()) {
        m_lastError = QString("Failed to clear high freq logs: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "[LogStorageEngine] Cleared high freq logs, affected rows:" << query.numRowsAffected();
    return true;
}

bool LogStorageEngine::insertOdometryLogs(const QVector<StorageLogEntry>& entries)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    if (entries.isEmpty()) {
        return true;
    }

    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO odometry_logs (timestamp, level, message, source, category, file_path, line_number)
        VALUES (?, ?, ?, ?, '', ?, ?)
    )");

    if (!m_database.transaction()) {
        m_lastError = QString("Failed to start transaction: %1").arg(m_database.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    int successCount = 0;
    for (const auto& entry : entries) {
        query.addBindValue(entry.timestamp.toMSecsSinceEpoch());
        query.addBindValue(static_cast<int>(entry.level));
        query.addBindValue(entry.message);
        query.addBindValue(entry.source);
        query.addBindValue(entry.filePath);
        query.addBindValue(entry.lineNumber);

        if (query.exec()) {
            successCount++;
        } else {
            qDebug() << "[LogStorageEngine] Failed to insert odometry log:" << query.lastError().text();
        }
    }

    if (!m_database.commit()) {
        m_lastError = QString("Failed to commit transaction: %1").arg(m_database.lastError().text());
        emit errorOccurred(m_lastError);
        m_database.rollback();
        return false;
    }

    return true;
}

// ============== 里程计日志清理 ==============

bool LogStorageEngine::clearOdometryLogs(const QDateTime& beforeTime)
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    QSqlQuery query(m_database);

    if (beforeTime.isValid()) {
        query.prepare("DELETE FROM odometry_logs WHERE timestamp < ?");
        query.addBindValue(beforeTime.toMSecsSinceEpoch());
    } else {
        query.prepare("DELETE FROM odometry_logs");
    }

    if (!query.exec()) {
        m_lastError = QString("Failed to clear odometry logs: %1").arg(query.lastError().text());
        emit errorOccurred(m_lastError);
        return false;
    }

    qDebug() << "[LogStorageEngine] Cleared odometry logs, affected rows:" << query.numRowsAffected();
    return true;
}

// ============== 保留策略管理 ==============

void LogStorageEngine::setRetentionPolicy(const RetentionPolicy& policy)
{
    m_retentionPolicy = policy;

    // 如果定时器已启动，更新间隔
    if (m_cleanupTimer && m_cleanupTimer->isActive()) {
        m_cleanupTimer->setInterval(m_retentionPolicy.cleanupIntervalHours * 3600 * 1000);
    }
}

RetentionPolicy LogStorageEngine::retentionPolicy() const
{
    return m_retentionPolicy;
}

// ============== 自动清理控制 ==============

void LogStorageEngine::startAutoCleanup()
{
    // 创建定时器（如果不存在）
    if (!m_cleanupTimer) {
        m_cleanupTimer = new QTimer(this);
        connect(m_cleanupTimer, &QTimer::timeout, this, [this]() {
            qDebug() << "[LogStorageEngine] Scheduled cleanup triggered";
            performCleanup();
        });
    }

    // 启动时立即执行一次清理
    qDebug() << "[LogStorageEngine] Performing startup cleanup...";
    performCleanup();

    // 启动定时清理（间隔单位：小时 -> 毫秒）
    int intervalMs = m_retentionPolicy.cleanupIntervalHours * 3600 * 1000;
    m_cleanupTimer->start(intervalMs);

    qDebug() << "[LogStorageEngine] Auto cleanup started, interval:"
             << m_retentionPolicy.cleanupIntervalHours << "hours";
}

void LogStorageEngine::stopAutoCleanup()
{
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
        delete m_cleanupTimer;
        m_cleanupTimer = nullptr;
        qDebug() << "[LogStorageEngine] Auto cleanup stopped";
    }
}

// ============== 核心清理逻辑 ==============

bool LogStorageEngine::performCleanup()
{
    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        emit errorOccurred(m_lastError);
        return false;
    }

    qint64 sizeBefore = getDatabaseSize();

    // 1. 按保留时间清理
    if (!cleanupByRetentionPolicy()) {
        return false;
    }

    // 2. 检查数据库大小，必要时按大小清理
    if (getDatabaseSizeMB() > m_retentionPolicy.maxDbSizeMB) {
        if (!cleanupBySizeLimit()) {
            return false;
        }
    }

    // 3. 执行 vacuum 压缩数据库
    vacuum();

    qint64 sizeAfter = getDatabaseSize();
    qint64 bytesFreed = sizeBefore - sizeAfter;

    qDebug() << "[LogStorageEngine] Cleanup completed, freed:" << bytesFreed << "bytes";

    return true;
}

bool LogStorageEngine::cleanupByRetentionPolicy()
{
    QDateTime now = QDateTime::currentDateTime();

    // 清理普通日志（保留 retentionDays 天）
    QDateTime logsCutoff = now.addDays(-m_retentionPolicy.retentionDays);
    if (!clearLogs(logsCutoff)) {
        qWarning() << "[LogStorageEngine] Failed to clear logs by retention policy";
    }

    // 清理高频日志（保留 highFreqRetentionDays 天）
    QDateTime highFreqCutoff = now.addDays(-m_retentionPolicy.highFreqRetentionDays);
    if (!clearHighFreqLogs(highFreqCutoff)) {
        qWarning() << "[LogStorageEngine] Failed to clear high freq logs by retention policy";
    }

    // 清理里程计日志（保留 odometryRetentionDays 天）
    QDateTime odometryCutoff = now.addDays(-m_retentionPolicy.odometryRetentionDays);
    if (!clearOdometryLogs(odometryCutoff)) {
        qWarning() << "[LogStorageEngine] Failed to clear odometry logs by retention policy";
    }

    return true;
}

bool LogStorageEngine::cleanupBySizeLimit()
{
    QWriteLocker locker(&m_lock);

    if (!m_initialized || !m_database.isOpen()) {
        m_lastError = "Database not initialized";
        return false;
    }

    // 目标大小为最大限制的 80%，留出缓冲空间
    qint64 targetSizeBytes = static_cast<qint64>(m_retentionPolicy.maxDbSizeMB * 0.8 * 1024 * 1024);

    QSqlQuery query(m_database);

    // 辅助 lambda：删除指定表最旧的一半数据
    auto deleteOldestHalf = [&](const QString &table) {
        QString sql = QString("DELETE FROM %1 WHERE id IN "
                              "(SELECT id FROM %1 ORDER BY timestamp ASC LIMIT "
                              "(SELECT COUNT(*)/2 FROM %1))").arg(table);
        return query.exec(sql);
    };

    // 优先删除里程计（占用大、价值低），然后高频日志，最后普通日志
    deleteOldestHalf("odometry_logs");
    if (getDatabaseSize() <= targetSizeBytes) {
        return true;
    }

    deleteOldestHalf("high_freq_logs");
    if (getDatabaseSize() <= targetSizeBytes) {
        return true;
    }

    deleteOldestHalf("logs");
    return true;
}

// ============== 数据库大小查询 ==============

qint64 LogStorageEngine::getDatabaseSize() const
{
    if (m_dbPath.isEmpty()) {
        return 0;
    }

    QFileInfo fileInfo(m_dbPath);
    return fileInfo.size();
}

double LogStorageEngine::getDatabaseSizeMB() const
{
    return static_cast<double>(getDatabaseSize()) / (1024.0 * 1024.0);
}
