#ifndef NAV2PARAMETERTHREAD_H
#define NAV2PARAMETERTHREAD_H

#include <rclcpp/rclcpp.hpp>

#include "basethread.h"
#include "roscontextmanager.h"
#include "threadsafequeue.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>

class Nav2ParameterThread : public BaseThread
{
    Q_OBJECT

public:
    enum class TaskType {
        Refresh,
        Apply,
        Reset,
        Discard
    };
    Q_ENUM(TaskType)

    enum class ParamStatus {
        Default,    // 默认状态（无特殊颜色）
        Pending,    // 待应用（蓝色）
        Success,    // 应用成功（绿色）
        Failed      // 应用失败（红色）
    };
    Q_ENUM(ParamStatus)

    struct ParamInfo {
        QString key;
        QString rosParamName;
        QStringList nodeNames;
        QVariant defaultValue;
        QVariant currentValue;
        QVariant pendingValue;
        bool modified;
        ParamStatus status;

        ParamInfo() : modified(false), status(ParamStatus::Default) {}

        QString primaryNode() const {
            return nodeNames.isEmpty() ? QString() : nodeNames.first();
        }
    };

    // 节点参数备份，用于多节点参数设置失败时的回滚
    struct NodeBackup {
        QString nodeName;       // 节点名称
        QString paramName;      // 参数名称
        QVariant originalValue; // 原始值备份
    };

    struct ParamTask {
        TaskType type;
        QString key;
        QVariant value;

        ParamTask() : type(TaskType::Refresh) {}
        explicit ParamTask(TaskType t) : type(t) {}
        ParamTask(TaskType t, const QString& k, const QVariant& v = QVariant())
            : type(t), key(k), value(v) {}
    };

public:
    explicit Nav2ParameterThread(QObject* parent = nullptr);
    ~Nav2ParameterThread() override;

    bool getParamInfo(const QString& key, ParamInfo& outInfo) const;
    QMap<QString, ParamInfo> getAllParams() const;
    bool setPendingValue(const QString& key, const QVariant& value);
    bool hasPendingChanges() const;
    void requestRefresh();
    void requestApply();
    void requestReset();
    void requestDiscard();

signals:
    void parameterRefreshed(bool success, const QString& message);
    void parameterApplied(bool success, const QString& message, const QStringList& appliedKeys, const QStringList& failedKeys);
    void operationFinished(const QString& operation, bool success, const QString& message);
    void operationProgress(const QString& operation, int current, int total, const QString& message);

protected:
    void initialize() override;
    void process() override;
    void cleanup() override;
    QVariant parameterToVariant(const rclcpp::Parameter& param);
    rclcpp::Parameter variantToParameter(const QVariant& value, const QString& paramName);

private:
    void registerParameters();
    QVariant getParameterFromNode(const QString& nodeName, const QString& paramName);
    bool setParameterOnNode(const QString& nodeName, const QString& paramName, const QVariant& value);
    void processTask(const ParamTask& task);
    void executeRefresh();
    void executeApply();
    void executeReset();
    void executeDiscard();
    bool hasParameter(const QString& key) const;

private:
    rclcpp::Node::SharedPtr m_node;
    // 使用基类的 m_executor（在 BaseThread 中定义），不需要创建额外的 executor
    QMap<QString, ParamInfo> m_params;
    ThreadSafeQueue<ParamTask> m_taskQueue;
    mutable QMutex m_paramsMutex;
    bool m_initialized;
};

#endif // NAV2PARAMETERTHREAD_H
