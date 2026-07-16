#ifndef LOGQUERYTASK_H
#define LOGQUERYTASK_H

#include <QDateTime>
#include <QVector>
#include <QString>
#include "logstorageengine.h"
#include "loglevel.h"

struct LogQueryParams {
    QString dbPath;
    QDateTime startTime;
    QDateTime endTime;
    QVector<int> selectedLevels;
    QString source;
    QString keyword;
    int limit;
    int offset;
    bool includeHighFreq;
    bool includeOdometry;

    LogQueryParams() : limit(-1), offset(0), includeHighFreq(false), includeOdometry(false) {}
};

struct LogQueryResult {
    QVector<StorageLogEntry> results;
    QString errorMessage;
    bool success;
    int totalCount;

    LogQueryResult() : success(false), totalCount(0) {}
};

class LogQueryTask
{
public:
    static LogQueryResult execute(const LogQueryParams& params);

private:
    static QVector<QVariant> buildWhereClause(const LogQueryParams& params, QString& sql);
    static void executeQuery(QSqlDatabase& db, const QString& sql, const QVector<QVariant>& bindValues, QVector<StorageLogEntry>& results);
};

#endif // LOGQUERYTASK_H
