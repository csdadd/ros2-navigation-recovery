#include "logquerytask.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QThread>

LogQueryResult LogQueryTask::execute(const LogQueryParams& params)
{
    LogQueryResult result;
    result.success = false;

    QString connectionName = QString("LogQueryThread_%1").arg(
        reinterpret_cast<quintptr>(QThread::currentThreadId())
    );

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(params.dbPath);

    if (!db.open()) {
        result.errorMessage = QString("Failed to open database: %1").arg(db.lastError().text());
        qWarning() << "[LogQueryTask]" << result.errorMessage;
        return result;
    }

    QVector<StorageLogEntry> allResults;

    QString sql = "SELECT timestamp, level, message, source, file_path, line_number FROM logs WHERE 1=1";
    QVector<QVariant> bindValues = buildWhereClause(params, sql);

    executeQuery(db, sql, bindValues, allResults);

    if (params.includeHighFreq) {
        QString hfSql = "SELECT timestamp, level, message, source, file_path, line_number FROM high_freq_logs WHERE 1=1";
        QVector<QVariant> hfBindValues = buildWhereClause(params, hfSql);
        executeQuery(db, hfSql, hfBindValues, allResults);
    }

    if (params.includeOdometry) {
        QString odomSql = "SELECT timestamp, level, message, source, file_path, line_number FROM odometry_logs WHERE 1=1";
        QVector<QVariant> odomBindValues = buildWhereClause(params, odomSql);
        executeQuery(db, odomSql, odomBindValues, allResults);
    }

    std::sort(allResults.begin(), allResults.end(), [](const StorageLogEntry& a, const StorageLogEntry& b) {
        return a.timestamp > b.timestamp;
    });

    int total = allResults.size();
    int start = params.offset > 0 ? qMin(params.offset, total) : 0;
    int end = params.limit > 0 ? qMin(start + params.limit, total) : total;

    result.results = allResults.mid(start, end - start);
    result.totalCount = total;

    qDebug() << "[LogQueryTask] Query completed, found" << result.results.size() << "logs (total:" << total << ")";

    db.close();
    QSqlDatabase::removeDatabase(connectionName);

    result.success = true;
    return result;
}

QVector<QVariant> LogQueryTask::buildWhereClause(const LogQueryParams& params, QString& sql)
{
    QVector<QVariant> bindValues;

    if (params.startTime.isValid()) {
        sql += " AND timestamp >= ?";
        bindValues.append(params.startTime.toMSecsSinceEpoch());
    }

    if (params.endTime.isValid()) {
        sql += " AND timestamp <= ?";
        bindValues.append(params.endTime.toMSecsSinceEpoch());
    }

    if (!params.selectedLevels.isEmpty()) {
        sql += " AND level IN (";
        for (int i = 0; i < params.selectedLevels.size(); ++i) {
            if (i > 0) sql += ",";
            sql += "?";
            bindValues.append(params.selectedLevels[i]);
        }
        sql += ")";
    }

    if (!params.source.isEmpty()) {
        sql += " AND source = ?";
        bindValues.append(params.source);
    }

    if (!params.keyword.isEmpty()) {
        sql += " AND message LIKE ?";
        bindValues.append(QString("%1%").arg(params.keyword));
    }

    return bindValues;
}

void LogQueryTask::executeQuery(QSqlDatabase& db, const QString& sql, const QVector<QVariant>& bindValues, QVector<StorageLogEntry>& results)
{
    QSqlQuery query(db);
    query.prepare(sql);

    for (const auto& value : bindValues) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        qWarning() << "[LogQueryTask] Failed to query:" << query.lastError().text();
        return;
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
}
