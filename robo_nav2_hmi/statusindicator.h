#ifndef STATUSINDICATOR_H
#define STATUSINDICATOR_H

#include <QString>
#include <QPointF>
#include <QColor>
#include <QDateTime>
#include <QList>

enum class StatusType
{
    Info,
    Warning,
    Error
};

struct StatusIndicator
{
    StatusType type;
    QPointF position;
    QString message;
    QDateTime timestamp;
};

class StatusIndicatorManager
{
public:
    StatusIndicatorManager();

    void addIndicator(const StatusIndicator& indicator);
    void removeIndicator(const QString& message);
    QList<StatusIndicator> getIndicators() const;
    void clear();
    QList<StatusIndicator> getIndicatorsByType(StatusType type) const;

private:
    QList<StatusIndicator> m_indicators;
};

#endif
