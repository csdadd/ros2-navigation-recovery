#ifndef LOGFILTERPROXYMODEL_H
#define LOGFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QSet>
#include <QDateTime>
#include <QRegularExpression>

class LogFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit LogFilterProxyModel(QObject* parent = nullptr);
    ~LogFilterProxyModel();

    void setLogLevelFilter(const QSet<int>& levels);
    QSet<int> logLevelFilter() const;

    void setKeywordFilter(const QString& keyword);
    QString keywordFilter() const;

    void setSourceFilter(const QString& source);
    QString sourceFilter() const;

    void setTimeRangeFilter(const QDateTime& start, const QDateTime& end);
    void clearTimeRangeFilter();
    QDateTime startTime() const;
    QDateTime endTime() const;

    void setRegExpFilter(const QString& pattern);
    void setUseRegExp(bool use);
    QString regExpFilter() const;
    bool useRegExp() const;

    void clearAllFilters();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QSet<int> m_logLevelFilter;
    QString m_keywordFilter;
    QString m_sourceFilter;
    QDateTime m_startTime;
    QDateTime m_endTime;
    QString m_regExpFilter;
    bool m_useRegExp;
    mutable QRegularExpression m_cachedRegExp;
};

#endif // LOGFILTERPROXYMODEL_H
