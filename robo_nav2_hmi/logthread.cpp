#include "logthread.h"
#include "logutils.h"
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

LogThread::LogThread(LogStorageEngine* storageEngine, QObject* parent)
    : BaseThread(parent)
    , m_storageEngine(storageEngine)
    , m_maxFileSize(DEFAULT_MAX_FILE_SIZE)
    , m_maxFileCount(DEFAULT_MAX_FILE_COUNT)
{
    m_threadName = "LogThread";

    m_logDirectory = QCoreApplication::applicationDirPath() + "/logs";
    QDir dir;
    if (!dir.exists(m_logDirectory)) {
        dir.mkpath(m_logDirectory);
    }

    m_currentLogDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    m_logFilePath = m_logDirectory + QString("/robot_%1.log").arg(m_currentLogDate);
}

LogThread::~LogThread()
{
    stopThread();
}

void LogThread::setLogFilePath(const QString& path)
{
    QMutexLocker locker(&m_fileMutex);
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    m_logFilePath = path;
    emit logFileChanged(path);
}

QString LogThread::getLogFilePath() const
{
    return m_logFilePath;
}

void LogThread::initialize()
{
    try {
        m_logFile.setFileName(m_logFilePath);
        if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            m_logStream.setDevice(&m_logFile);
            m_logStream.setCodec(QTextCodec::codecForName("UTF-8"));
        } else {
            emit threadError(QString("Failed to open log file: %1").arg(m_logFilePath));
        }

        if (m_storageEngine && !m_storageEngine->isInitialized()) {
            emit threadError("LogStorageEngine not initialized");
        }

        emit logFileChanged(m_logFilePath);
        emit logMessage("LogThread initialized successfully", LOG_INFO);

    } catch (const std::exception& e) {
        emit threadError(QString("Failed to initialize LogThread: %1").arg(e.what()));
        throw;
    }
}

void LogThread::process()
{
    processLogQueue();
}

void LogThread::cleanup()
{
    processLogQueue();

    if (!m_pendingBatchEntries.isEmpty() && m_storageEngine && m_storageEngine->isInitialized()) {
        if (!m_storageEngine->insertLogs(m_pendingBatchEntries)) {
            qWarning() << "[LogThread] Failed to insert remaining logs to database during cleanup";
        }
        m_pendingBatchEntries.clear();
    }

    QMutexLocker locker(&m_fileMutex);
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }

    emit logMessage("LogThread cleanup completed", LOG_INFO);
}

void LogThread::writeLog(const QString& message, LogLevel level)
{
    LogEntry entry(message, level, QDateTime::currentDateTime(), "Application");
    m_logQueue.enqueue(entry);
}

void LogThread::writeLogEntry(const LogEntry& entry)
{
    m_logQueue.enqueue(entry);
}

void LogThread::processLogQueue()
{
    QVector<StorageLogEntry> highFreqBatchEntries;
    QVector<StorageLogEntry> odometryBatchEntries;
    LogEntry entry;

    while (m_logQueue.tryDequeue(entry, 0)) {
        if (entry.level == LogLevel::HIGHFREQ) {
            StorageLogEntry storageEntry(
                entry.message,
                entry.level,
                entry.timestamp,
                entry.source
            );
            highFreqBatchEntries.append(storageEntry);

            if (highFreqBatchEntries.size() >= 4) {
                if (m_storageEngine && m_storageEngine->isInitialized()) {
                    if (!m_storageEngine->insertHighFreqLogs(highFreqBatchEntries)) {
                        qWarning() << "[LogThread] Failed to insert high freq logs to database, will retry next cycle";
                    }
                }
                highFreqBatchEntries.clear();
            }
        } else if (entry.level == LogLevel::ODOMETRY) {
            StorageLogEntry storageEntry(
                entry.message,
                entry.level,
                entry.timestamp,
                entry.source
            );
            odometryBatchEntries.append(storageEntry);

            if (odometryBatchEntries.size() >= 100) {
                if (m_storageEngine && m_storageEngine->isInitialized()) {
                    if (!m_storageEngine->insertOdometryLogs(odometryBatchEntries)) {
                        qWarning() << "[LogThread] Failed to insert odometry logs to database, will retry next cycle";
                    }
                }
                odometryBatchEntries.clear();
            }
        } else {
            writeToFile(entry.message, entry.level, entry.timestamp, entry.source);

            StorageLogEntry storageEntry(
                entry.message,
                entry.level,
                entry.timestamp,
                entry.source
            );
            m_pendingBatchEntries.append(storageEntry);
        }
    }

    if (!highFreqBatchEntries.isEmpty() && m_storageEngine && m_storageEngine->isInitialized()) {
        if (!m_storageEngine->insertHighFreqLogs(highFreqBatchEntries)) {
            qWarning() << "[LogThread] Failed to insert remaining high freq logs to database, will retry next cycle";
        }
    }

    if (!odometryBatchEntries.isEmpty() && m_storageEngine && m_storageEngine->isInitialized()) {
        if (!m_storageEngine->insertOdometryLogs(odometryBatchEntries)) {
            qWarning() << "[LogThread] Failed to insert remaining odometry logs to database, will retry next cycle";
        }
    }

    QDateTime now = QDateTime::currentDateTime();
    if (!m_lastBatchWriteTime.isValid()) {
        m_lastBatchWriteTime = now;
    }

    if (!m_pendingBatchEntries.isEmpty() && m_storageEngine && m_storageEngine->isInitialized()) {
        qint64 elapsedMs = m_lastBatchWriteTime.msecsTo(now);
        if (elapsedMs >= BATCH_WRITE_INTERVAL_MS) {
            if (!m_storageEngine->insertLogs(m_pendingBatchEntries)) {
                qWarning() << "[LogThread] Failed to insert logs to database, will retry next cycle";
            } else {
                m_pendingBatchEntries.clear();
            }
            m_lastBatchWriteTime = now;
        }
    }
}

void LogThread::writeToFile(const QString& message, LogLevel level, const QDateTime& timestamp, const QString& source)
{
    QMutexLocker locker(&m_fileMutex);

    if (!m_logFile.isOpen()) {
        return;
    }

    checkDateRollover();
    rotateLogFile();

    QString formattedMsg = formatLogMessage(message, level, timestamp, source);
    m_logStream << formattedMsg << Qt::endl;
    m_logStream.flush();
}

void LogThread::checkDateRollover()
{
    QString currentDate = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    if (currentDate != m_currentLogDate) {
        m_logFile.close();
        m_logStream.setDevice(nullptr);

        m_currentLogDate = currentDate;
        m_logFilePath = m_logDirectory + QString("/robot_%1.log").arg(m_currentLogDate);

        m_logFile.setFileName(m_logFilePath);
        if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
            m_logStream.setDevice(&m_logFile);
            m_logStream.setCodec(QTextCodec::codecForName("UTF-8"));
            emit logFileChanged(m_logFilePath);
        }
    }
}

void LogThread::rotateLogFile()
{
    if (m_logFile.size() >= m_maxFileSize) {
        m_logFile.close();
        m_logStream.setDevice(nullptr);

        QFileInfo fileInfo(m_logFilePath);
        QString baseName = fileInfo.baseName();
        QString extension = fileInfo.completeSuffix();

        for (int i = m_maxFileCount - 1; i > 0; i--) {
            QString oldName = QString("%1/%2.%3.%4").arg(m_logDirectory, baseName).arg(i).arg(extension);
            QString newName = QString("%1/%2.%3.%4").arg(m_logDirectory, baseName).arg(i + 1).arg(extension);

            QDir dir;
            if (i == m_maxFileCount - 1) {
                dir.remove(newName);
            }
            dir.rename(oldName, newName);
        }

        QString backupName = QString("%1/%2.1.%3").arg(m_logDirectory, baseName, extension);
        QDir dir;
        dir.rename(m_logFilePath, backupName);

        m_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
        m_logStream.setDevice(&m_logFile);
        m_logStream.setCodec(QTextCodec::codecForName("UTF-8"));
    }
}

QString LogThread::formatLogMessage(const QString& message, LogLevel level, const QDateTime& timestamp, const QString& source)
{
    QString timeStr = timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = LogUtils::levelToString(level);

    if (source.isEmpty()) {
        return QString("[%1] [%2] %3").arg(timeStr, levelStr, message);
    } else {
        return QString("[%1] [%2] [%3] %4").arg(timeStr, levelStr, source, message);
    }
}

LogStorageEngine* LogThread::getStorageEngine() const
{
    return m_storageEngine;
}
