#include "statusindicator.h"
#include <QDebug>

StatusIndicatorManager::StatusIndicatorManager()
{
}

void StatusIndicatorManager::addIndicator(const StatusIndicator& indicator)
{
    if (indicator.message.isEmpty()) {
        qWarning() << "[StatusIndicatorManager] 添加状态指示器失败 - 消息为空";
        return;
    }

    m_indicators.append(indicator);
    qDebug() << "[StatusIndicatorManager] 添加状态指示器成功 - 类型:" << static_cast<int>(indicator.type) << "消息:" << indicator.message;
}

void StatusIndicatorManager::removeIndicator(const QString& message)
{
    for (int i = 0; i < m_indicators.size(); ++i) {
        if (m_indicators[i].message == message) {
            m_indicators.removeAt(i);
            qDebug() << "[StatusIndicatorManager] 删除状态指示器成功 - 消息:" << message;
            return;
        }
    }

    qWarning() << "[StatusIndicatorManager] 删除状态指示器失败 - 未找到状态指示器:" << message;
}

QList<StatusIndicator> StatusIndicatorManager::getIndicators() const
{
    return m_indicators;
}

void StatusIndicatorManager::clear()
{
    m_indicators.clear();
    qDebug() << "[StatusIndicatorManager] 清除所有状态指示器";
}

QList<StatusIndicator> StatusIndicatorManager::getIndicatorsByType(StatusType type) const
{
    QList<StatusIndicator> result;
    for (const StatusIndicator& indicator : m_indicators) {
        if (indicator.type == type) {
            result.append(indicator);
        }
    }
    return result;
}
