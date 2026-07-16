#include "nav2parameterthread.h"
#include <chrono>

Nav2ParameterThread::Nav2ParameterThread(QObject* parent)
    : BaseThread(parent)
    , m_initialized(false)
{
    m_threadName = "Nav2ParameterThread";
    qDebug() << "[Nav2ParameterThread] 构造函数";
}

Nav2ParameterThread::~Nav2ParameterThread()
{
    stopThread();
}

void Nav2ParameterThread::initialize()
{
    try {
        qDebug() << "[Nav2ParameterThread] 开始初始化";

        ROSContextManager::instance().initialize();

        m_node = std::make_shared<rclcpp::Node>("nav2_parameter_client");

        registerParameters();

        // 从参数服务器读取所有参数的当前值，同时作为默认值
        for (auto& key : m_params.keys()) {
            QVariant value = getParameterFromNode(m_params[key].primaryNode(), m_params[key].rosParamName);
            if (value.isValid()) {
                m_params[key].defaultValue = value;
                m_params[key].currentValue = value;
                m_params[key].pendingValue = value;
                m_params[key].modified = false;
                qDebug() << "[Nav2ParameterThread] 读取参数" << key << "=" << value;
            } else {
                qWarning() << "[Nav2ParameterThread] 无法读取参数:" << key;
            }
        }

        // 将节点添加到基类 executor
        if (m_executor) {
            m_executor->add_node(m_node);
        }

        m_initialized = true;
        qDebug() << "[Nav2ParameterThread] 初始化成功";
        emit logMessage("Nav2ParameterThread initialized successfully", LogLevel::INFO);

    } catch (const std::exception& e) {
        QString error = QString("Failed to initialize Nav2ParameterThread: %1").arg(e.what());
        qCritical() << "[Nav2ParameterThread]" << error;
        emit threadError(error);
        emit operationFinished("initialize", false, error);
        throw;
    }
}

void Nav2ParameterThread::process()
{
    ParamTask task;
    while (m_taskQueue.tryDequeue(task, 10)) {
        processTask(task);
    }
}

void Nav2ParameterThread::cleanup()
{
    qDebug() << "[Nav2ParameterThread] 清理资源";

    // 从基类 executor 中移除节点
    if (m_executor && m_node) {
        m_executor->remove_node(m_node);
    }

    m_node.reset();

    QMutexLocker paramsLocker(&m_paramsMutex);
    m_params.clear();

    m_initialized = false;

    qDebug() << "[Nav2ParameterThread] 清理完成";
    emit logMessage("Nav2ParameterThread cleanup completed", LogLevel::INFO);
}

bool Nav2ParameterThread::getParamInfo(const QString& key, ParamInfo& outInfo) const
{
    QMutexLocker locker(&m_paramsMutex);

    auto it = m_params.find(key);
    if (it != m_params.end()) {
        outInfo = it.value();
        return true;
    }
    return false;
}

QMap<QString, Nav2ParameterThread::ParamInfo> Nav2ParameterThread::getAllParams() const
{
    QMutexLocker locker(&m_paramsMutex);
    return m_params;
}

bool Nav2ParameterThread::setPendingValue(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_paramsMutex);

    auto it = m_params.find(key);
    if (it == m_params.end()) {
        qWarning() << "[Nav2ParameterThread] 参数不存在:" << key;
        return false;
    }

    it->pendingValue = value;
    it->modified = true;

    qDebug() << "[Nav2ParameterThread] 设置待应用值:" << key << "=" << value;
    return true;
}

bool Nav2ParameterThread::hasPendingChanges() const
{
    QMutexLocker locker(&m_paramsMutex);

    for (const auto& param : m_params) {
        if (param.modified) {
            return true;
        }
    }
    return false;
}

void Nav2ParameterThread::requestRefresh()
{
    m_taskQueue.enqueue(ParamTask(TaskType::Refresh));
    qDebug() << "[Nav2ParameterThread] 已添加刷新任务到队列";
}

void Nav2ParameterThread::requestApply()
{
    m_taskQueue.enqueue(ParamTask(TaskType::Apply));
    qDebug() << "[Nav2ParameterThread] 已添加应用任务到队列";
}

void Nav2ParameterThread::requestReset()
{
    m_taskQueue.enqueue(ParamTask(TaskType::Reset));
    qDebug() << "[Nav2ParameterThread] 已添加重置任务到队列";
}

void Nav2ParameterThread::requestDiscard()
{
    m_taskQueue.enqueue(ParamTask(TaskType::Discard));
    qDebug() << "[Nav2ParameterThread] 已添加丢弃任务到队列";
}

void Nav2ParameterThread::registerParameters()
{
    qDebug() << "[Nav2ParameterThread] 开始注册参数";

    ParamInfo p;

    // ===== 速度控制参数 (controller_server → FollowPath) =====
    // 最大线速度
    p.key = "maxVelXSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.desired_linear_vel";
    m_params[p.key] = p;

    // 接近目标最小速度
    p.key = "minVelXSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.min_approach_linear_velocity";
    m_params[p.key] = p;

    // 最大角加速度
    p.key = "maxVelThetaSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.max_angular_accel";
    m_params[p.key] = p;

    // ===== 前视距离参数 =====
    p.key = "lookaheadDistSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.lookahead_dist";
    m_params[p.key] = p;

    // 最大前视距离
    p.key = "smootherMaxVelSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.max_lookahead_dist";
    m_params[p.key] = p;

    // 最小前视距离
    p.key = "smootherMinVelSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.min_lookahead_dist";
    m_params[p.key] = p;

    // ===== 代价缩放参数 =====
    p.key = "smootherMaxAccelSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.cost_scaling_gain";
    m_params[p.key] = p;

    p.key = "smootherMaxDecelSpinBox";
    p.nodeNames = QStringList() << "/controller_server";
    p.rosParamName = "FollowPath.cost_scaling_dist";
    m_params[p.key] = p;

    // ===== 代价地图参数 (local_costmap / global_costmap) =====
    p.key = "localInflationRadiusSpinBox";
    p.nodeNames = QStringList() << "/local_costmap/local_costmap";
    p.rosParamName = "inflation_layer.inflation_radius";
    m_params[p.key] = p;

    p.key = "localCostScalingFactorSpinBox";
    p.nodeNames = QStringList() << "/local_costmap/local_costmap";
    p.rosParamName = "inflation_layer.cost_scaling_factor";
    m_params[p.key] = p;

    p.key = "globalInflationRadiusSpinBox";
    p.nodeNames = QStringList() << "/global_costmap/global_costmap";
    p.rosParamName = "inflation_layer.inflation_radius";
    m_params[p.key] = p;

    p.key = "globalCostScalingFactorSpinBox";
    p.nodeNames = QStringList() << "/global_costmap/global_costmap";
    p.rosParamName = "inflation_layer.cost_scaling_factor";
    m_params[p.key] = p;

    // ===== 机器人参数 =====
    p.key = "robotRadiusSpinBox";
    p.nodeNames = QStringList() << "/local_costmap/local_costmap" << "/global_costmap/global_costmap";
    p.rosParamName = "robot_radius";
    m_params[p.key] = p;

    qDebug() << "[Nav2ParameterThread] 参数注册完成，共" << m_params.size() << "个参数";
}

QVariant Nav2ParameterThread::getParameterFromNode(const QString& nodeName, const QString& paramName)
{
    try {
        auto client = std::make_shared<rclcpp::SyncParametersClient>(m_node, nodeName.toStdString());

        if (!client->wait_for_service(std::chrono::seconds(2))) {
            qWarning() << "[Nav2ParameterThread] 参数服务不可用，节点:" << nodeName;
            return QVariant();
        }

        auto parameters = client->get_parameters({paramName.toStdString()}, std::chrono::seconds(2));

        if (parameters.empty()) {
            qWarning() << "[Nav2ParameterThread] 参数未找到:" << paramName << "节点:" << nodeName;
            return QVariant();
        }

        auto& param = parameters[0];
        switch (param.get_type()) {
            case rclcpp::ParameterType::PARAMETER_DOUBLE:
                return param.as_double();
            case rclcpp::ParameterType::PARAMETER_INTEGER:
                return static_cast<int>(param.as_int());
            case rclcpp::ParameterType::PARAMETER_BOOL:
                return param.as_bool();
            case rclcpp::ParameterType::PARAMETER_DOUBLE_ARRAY: {
                // velocity_smoother 参数是数组类型，取第一个元素
                auto arr = param.as_double_array();
                if (!arr.empty()) {
                    return arr[0];
                } else {
                    qWarning() << "[Nav2ParameterThread] 参数数组为空:" << paramName;
                    return QVariant();
                }
            }
            default:
                qWarning() << "[Nav2ParameterThread] 不支持的参数类型:" << paramName;
                return QVariant();
        }

    } catch (const std::exception& e) {
        qCritical() << "[Nav2ParameterThread] getParameterFromNode 异常:" << e.what();
        return QVariant();
    }
}

bool Nav2ParameterThread::setParameterOnNode(const QString& nodeName, const QString& paramName, const QVariant& value)
{
    try {
        auto client = std::make_shared<rclcpp::SyncParametersClient>(m_node, nodeName.toStdString());

        if (!client->wait_for_service(std::chrono::seconds(2))) {
            qWarning() << "[Nav2ParameterThread] 参数服务不可用，节点:" << nodeName;
            return false;
        }

        std::vector<rclcpp::Parameter> params;

        // 处理 velocity_smoother 的数组参数
        if (nodeName == "/velocity_smoother") {
            // velocity_smoother 参数是 double[] 类型 [x, y, theta]
            // 需要先读取当前值，然后修改对应索引
            auto currentParams = client->get_parameters({paramName.toStdString()}, std::chrono::seconds(2));
            if (!currentParams.empty() && currentParams[0].get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE_ARRAY) {
                auto currentArray = currentParams[0].as_double_array();
                double newValue = value.toDouble();

                // 修改数组：只修改索引 0 (x 方向)，保留索引 1 (y) 和索引 2 (theta) 的原值
                currentArray[0] = newValue;
                // y 方向 (索引 1) 保持 0.0 (差速/阿克曼机器人)
                currentArray[1] = 0.0;
                // theta 方向 (索引 2) 保留原值不变

                params.push_back(rclcpp::Parameter(paramName.toStdString(), currentArray));
            } else {
                // 读取失败时无法保留 theta 方向的原值，直接返回失败
                qWarning() << "[Nav2ParameterThread] 无法读取当前数组值:" << paramName
                           << "无法保留 theta 原值，取消写入操作";
                return false;
            }
        } else {
            // 普通 double 参数
            if (value.type() == QVariant::Double) {
                params.push_back(rclcpp::Parameter(paramName.toStdString(), value.toDouble()));
            } else if (value.type() == QVariant::Int || value.type() == QVariant::LongLong) {
                params.push_back(rclcpp::Parameter(paramName.toStdString(), value.toInt()));
            } else if (value.type() == QVariant::Bool) {
                params.push_back(rclcpp::Parameter(paramName.toStdString(), value.toBool()));
            } else {
                qWarning() << "[Nav2ParameterThread] 不支持的 QVariant 类型:" << paramName;
                return false;
            }
        }

        auto results = client->set_parameters(params, std::chrono::seconds(2));

        if (results.empty() || !results[0].successful) {
            qWarning() << "[Nav2ParameterThread] 设置参数失败:" << paramName;
            if (!results.empty()) {
                qWarning() << "  原因:" << QString::fromStdString(results[0].reason);
            }
            return false;
        }

        // 回读验证：确认参数确实被设置成功
        auto verifyParams = client->get_parameters({paramName.toStdString()}, std::chrono::seconds(2));
        if (verifyParams.empty()) {
            qWarning() << "[Nav2ParameterThread] 回读验证失败：无法获取参数" << paramName;
            return false;
        }

        auto& verifyParam = verifyParams[0];

        if (nodeName == "/velocity_smoother") {
            // 对于数组参数，验证第一个元素是否匹配
            if (verifyParam.get_type() == rclcpp::ParameterType::PARAMETER_DOUBLE_ARRAY) {
                auto arr = verifyParam.as_double_array();
                if (arr.empty() || !qFuzzyCompare(arr[0], value.toDouble())) {
                    qWarning() << "[Nav2ParameterThread] 回读验证失败：值不匹配。期望:" << value.toDouble() << "实际:" << (arr.empty() ? 0.0 : arr[0]);
                    return false;
                }
            } else {
                qWarning() << "[Nav2ParameterThread] 回读验证失败：参数类型不匹配";
                return false;
            }
        } else {
            // 对于普通参数，直接比较值
            QVariant actualValue = parameterToVariant(verifyParam);
            if (value.type() == QVariant::Double) {
                if (!qFuzzyCompare(actualValue.toDouble(), value.toDouble())) {
                    qWarning() << "[Nav2ParameterThread] 回读验证失败：值不匹配。期望:" << value.toDouble() << "实际:" << actualValue.toDouble();
                    return false;
                }
            } else {
                if (actualValue != value) {
                    qWarning() << "[Nav2ParameterThread] 回读验证失败：值不匹配。期望:" << value << "实际:" << actualValue;
                    return false;
                }
            }
        }

        qDebug() << "[Nav2ParameterThread] 参数设置成功并已验证:" << paramName << "=" << value;
        return true;

    } catch (const std::exception& e) {
        qCritical() << "[Nav2ParameterThread] setParameterOnNode 异常:" << e.what();
        return false;
    }
}

void Nav2ParameterThread::processTask(const ParamTask& task)
{
    qDebug() << "[Nav2ParameterThread] 处理任务，类型:" << static_cast<int>(task.type);

    switch (task.type) {
        case TaskType::Refresh:
            executeRefresh();
            break;
        case TaskType::Apply:
            executeApply();
            break;
        case TaskType::Reset:
            executeReset();
            break;
        case TaskType::Discard:
            executeDiscard();
            break;
        default:
            qWarning() << "[Nav2ParameterThread] 未知任务类型:" << static_cast<int>(task.type);
            break;
    }
}

void Nav2ParameterThread::executeRefresh()
{
    QMutexLocker locker(&m_paramsMutex);

    int successCount = 0;
    int failCount = 0;
    int total = m_params.size();

    for (auto& param : m_params) {
        QVariant value = getParameterFromNode(param.primaryNode(), param.rosParamName);
        if (value.isValid()) {
            param.currentValue = value;
            param.pendingValue = value;
            param.modified = false;
            param.status = ParamStatus::Default;
            successCount++;
        } else {
            qWarning() << "[Nav2ParameterThread] 刷新参数失败:" << param.key;
            failCount++;
        }
    }

    locker.unlock();

    QString message = QString("刷新完成: 成功 %1, 失败 %2").arg(successCount).arg(failCount);
    qDebug() << "[Nav2ParameterThread]" << message;
    emit parameterRefreshed(failCount == 0, message);
}

void Nav2ParameterThread::executeApply()
{
    QMutexLocker locker(&m_paramsMutex);

    QStringList appliedKeys;
    QStringList failedKeys;
    int modifiedCount = 0;

    // 阶段1：备份所有待修改参数的原值
    QMap<QString, QVector<NodeBackup>> allBackups;  // key -> 节点备份列表

    for (auto& param : m_params) {
        if (!param.modified) {
            continue;
        }
        modifiedCount++;

        QVector<NodeBackup> backups;
        for (const auto& nodeName : param.nodeNames) {
            NodeBackup backup;
            backup.nodeName = nodeName;
            backup.paramName = param.rosParamName;
            backup.originalValue = getParameterFromNode(nodeName, param.rosParamName);

            if (!backup.originalValue.isValid()) {
                qWarning() << "[Nav2ParameterThread] 备份失败，取消应用:" << param.key;
                emit operationFinished("apply", false, QString("备份参数失败: %1").arg(param.key));
                return;
            }
            backups.append(backup);
        }
        allBackups[param.key] = backups;
    }

    if (modifiedCount == 0) {
        qDebug() << "[Nav2ParameterThread] 没有待应用的更改";
        emit operationFinished("apply", true, "没有待应用的更改");
        return;
    }

    // 阶段2：应用参数修改
    for (auto& param : m_params) {
        if (!param.modified) {
            continue;
        }

        bool allSuccess = true;
        const QVector<NodeBackup>& backups = allBackups[param.key];
        int lastSuccessIndex = -1;  // 记录最后一个成功写入的节点索引

        // 依次写入所有节点
        for (int i = 0; i < param.nodeNames.size(); ++i) {
            const auto& nodeName = param.nodeNames[i];
            if (setParameterOnNode(nodeName, param.rosParamName, param.pendingValue)) {
                lastSuccessIndex = i;
            } else {
                qWarning() << "[Nav2ParameterThread] 应用参数失败:" << param.key
                           << "节点:" << nodeName;
                allSuccess = false;
                break;
            }
        }

        if (allSuccess) {
            // 全部成功：更新本地状态
            param.currentValue = param.pendingValue;
            param.modified = false;
            param.status = ParamStatus::Success;
            appliedKeys.append(param.key);
        } else {
            // 失败：回滚已写入的节点
            qWarning() << "[Nav2ParameterThread] 开始回滚已写入的节点:" << param.key;
            for (int i = 0; i <= lastSuccessIndex; ++i) {
                const auto& backup = backups[i];
                if (!setParameterOnNode(backup.nodeName, backup.paramName, backup.originalValue)) {
                    qCritical() << "[Nav2ParameterThread] 回滚失败:" << backup.nodeName
                                << "参数可能处于不一致状态";
                }
            }
            param.status = ParamStatus::Failed;
            failedKeys.append(param.key);
        }
    }

    locker.unlock();

    QString message = QString("应用完成: 成功 %1, 失败 %2")
                          .arg(appliedKeys.size())
                          .arg(failedKeys.size());
    qDebug() << "[Nav2ParameterThread]" << message;
    emit parameterApplied(failedKeys.isEmpty(), message, appliedKeys, failedKeys);
    emit operationFinished("apply", failedKeys.isEmpty(), message);
}

void Nav2ParameterThread::executeReset()
{
    QMutexLocker locker(&m_paramsMutex);

    int resetCount = 0;
    for (auto& param : m_params) {
        param.pendingValue = param.defaultValue;
        param.modified = true;
        resetCount++;
    }

    locker.unlock();

    QString message = QString("已重置 %1 个参数为默认值，请点击'应用更改'以生效").arg(resetCount);
    qDebug() << "[Nav2ParameterThread]" << message;
    emit operationFinished("reset", true, message);
}

void Nav2ParameterThread::executeDiscard()
{
    QMutexLocker locker(&m_paramsMutex);

    int discardedCount = 0;
    for (auto& param : m_params) {
        if (param.modified) {
            param.pendingValue = param.currentValue;
            param.modified = false;
            discardedCount++;
        }
    }

    qDebug() << "[Nav2ParameterThread] 丢弃了" << discardedCount << "个待应用更改";
    emit operationFinished("discard", true, QString("丢弃了 %1 个待应用更改").arg(discardedCount));
}

bool Nav2ParameterThread::hasParameter(const QString& key) const
{
    QMutexLocker locker(&m_paramsMutex);
    return m_params.contains(key);
}

QVariant Nav2ParameterThread::parameterToVariant(const rclcpp::Parameter& param)
{
    switch (param.get_type()) {
        case rclcpp::ParameterType::PARAMETER_DOUBLE:
            return param.as_double();
        case rclcpp::ParameterType::PARAMETER_INTEGER:
            return static_cast<int>(param.as_int());
        case rclcpp::ParameterType::PARAMETER_BOOL:
            return param.as_bool();
        case rclcpp::ParameterType::PARAMETER_DOUBLE_ARRAY: {
            auto arr = param.as_double_array();
            return !arr.empty() ? arr[0] : QVariant();
        }
        case rclcpp::ParameterType::PARAMETER_STRING:
            return QString::fromStdString(param.as_string());
        default:
            qWarning() << "[Nav2ParameterThread] 不支持的参数类型转换";
            return QVariant();
    }
}

rclcpp::Parameter Nav2ParameterThread::variantToParameter(const QVariant& value, const QString& paramName)
{
    if (value.type() == QVariant::Double) {
        return rclcpp::Parameter(paramName.toStdString(), value.toDouble());
    } else if (value.type() == QVariant::Int || value.type() == QVariant::LongLong) {
        return rclcpp::Parameter(paramName.toStdString(), value.toInt());
    } else if (value.type() == QVariant::Bool) {
        return rclcpp::Parameter(paramName.toStdString(), value.toBool());
    } else if (value.type() == QVariant::String) {
        return rclcpp::Parameter(paramName.toStdString(), value.toString().toStdString());
    } else {
        qWarning() << "[Nav2ParameterThread] 不支持的 QVariant 类型转换:" << value.type();
        return rclcpp::Parameter(paramName.toStdString(), rclcpp::ParameterValue());
    }
}
