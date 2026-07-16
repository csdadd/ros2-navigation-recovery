#include "coordinatetransformer.h"
#include "logutils.h"
#include "logquerytask.h"
#include <QPointF>
#include <QMetaType>
#include <QSet>
#include <QTextCodec>

Q_DECLARE_METATYPE(CoordinateTransformer)

#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QDateTime>
#include <QFileDialog>
#include <QTimer>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <algorithm>
#include <QStandardPaths>
#include "maploader.h"
#include "mapconverter.h"
#include "mapmarker.h"
#include "roscontextmanager.h"
#include <nav_msgs/msg/path.hpp>
#include <ament_index_cpp/get_package_prefix.hpp>
#include <ament_index_cpp/get_package_share_directory.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_targetX(0.0)
    , m_targetY(0.0)
    , m_targetYaw(0.0)
    , m_hasTarget(false)
    , m_startTime(QDateTime::currentDateTime())
    , m_initialDistance(0.0)
{
    ui->setupUi(this);
}

bool MainWindow::initialize()
{
    qRegisterMetaType<CoordinateTransformer>();
    qRegisterMetaType<RenderData>();
    qRegisterMetaType<User>();

    if (!initializeUserSystem()) {
        return false;
    }

    if (!handleLogin()) {  // ← 先登录
        return false;
    }

    initializeLogSystem();
    initializeROSContext();
    initializeThreads();
    initializeUI();
    connectSignals();
    startAllThreads();     // ← 登录成功后再启动线程

    return true;
}

bool MainWindow::initializeUserSystem()
{
    m_userStorageEngine = std::make_unique<UserStorageEngine>(this);
    if (!m_userStorageEngine->initialize()) {
        qCritical() << "[MainWindow] 错误：用户存储引擎初始化失败！";
        QMessageBox::critical(this, "错误", "用户存储引擎初始化失败！\n\n程序无法继续运行。");
        return false;
    }

    m_userAuthManager = std::make_unique<UserAuthManager>(this);
    if (!m_userAuthManager->initialize(m_userStorageEngine.get())) {
        qCritical() << "[MainWindow] 错误：用户认证管理器初始化失败！";
        QMessageBox::critical(this, "错误", "用户认证管理器初始化失败！\n\n程序无法继续运行。");
        return false;
    }

    m_loginDialog = std::make_unique<LoginDialog>(m_userAuthManager.get(), this);
    return true;
}

void MainWindow::initializeLogSystem()
{
    m_logTableModel = std::make_unique<LogTableModel>(this);
    m_logTableModel->setBatchUpdateEnabled(true);
    m_logTableModel->setBatchUpdateInterval(200);
    m_logFilterProxyModel = std::make_unique<LogFilterProxyModel>(this);
    m_logFilterProxyModel->setSourceModel(m_logTableModel.get());
    ui->logTableView->setModel(m_logFilterProxyModel.get());

    ui->logTableView->setColumnWidth(0, 180);
    ui->logTableView->setColumnWidth(1, 80);
    ui->logTableView->setColumnWidth(2, 120);
    ui->logTableView->horizontalHeader()->setStretchLastSection(true);

    m_historyLogTableModel = std::make_unique<HistoryLogTableModel>(this);
    m_historyLogFilterProxyModel = std::make_unique<LogFilterProxyModel>(this);
    m_historyLogFilterProxyModel->setSourceModel(m_historyLogTableModel.get());
    ui->historyLogTableView->setModel(m_historyLogFilterProxyModel.get());

    ui->historyLogTableView->setColumnWidth(0, 180);
    ui->historyLogTableView->setColumnWidth(1, 80);
    ui->historyLogTableView->setColumnWidth(2, 120);
    ui->historyLogTableView->horizontalHeader()->setStretchLastSection(true);

    ui->endDateTimeEdit->setDateTime(QDateTime::currentDateTime());
    ui->startDateTimeEdit->setDateTime(QDateTime::currentDateTime().addDays(-1));
}

void MainWindow::initializeROSContext()
{
    ROSContextManager::instance().initialize();

    auto temp_node = std::make_shared<rclcpp::Node>("temp_node");
    m_mapPath = getMapPathFromRosParam(temp_node).toStdString();
}

void MainWindow::initializeUI()
{
    ui->nav2MapLayout->addWidget(m_nav2ViewWidget.get());

    QTimer* currentTimeTimer = new QTimer(this);
    connect(currentTimeTimer, &QTimer::timeout, this, &MainWindow::updateCurrentTime);
    currentTimeTimer->start(1000);
}

bool MainWindow::handleLogin()
{
    // 尝试从环境变量自动登录（仅用于测试）
    QString auto_username = qgetenv("WHEELTEC_USERNAME");
    QString auto_password = qgetenv("WHEELTEC_PASSWORD");
    if (!auto_username.isEmpty() && !auto_password.isEmpty()) {
        qDebug() << "[MainWindow] 尝试自动登录...";
        if (m_userAuthManager->login(auto_username, auto_password)) {
            qDebug() << "[MainWindow] 自动登录成功:" << m_userAuthManager->getCurrentUsername();
            return true;
        }
        qWarning() << "[MainWindow] 自动登录失败:" << m_userAuthManager->getLastError();
    }

    // [测试模式] 注释登录功能，直接进入主界面
    // 检查登录状态，未登录则显示登录对话框
    if (!m_userAuthManager->isLoggedIn()) {
        qDebug() << "[MainWindow] 用户未登录，显示登录对话框";
        int maxRetries = 3;
        int retryCount = 0;
    
        while (retryCount < maxRetries) {
            m_loginDialog->exec();
    
            if (m_userAuthManager->isLoggedIn()) {
                qDebug() << "[MainWindow] 登录成功:" << m_userAuthManager->getCurrentUsername();
                break;
            }
    
            retryCount++;
    
            if (retryCount >= maxRetries) {
                qCritical() << "[MainWindow] 错误：登录失败次数过多";
                return false;
            }
    
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("登录失败"),
                tr("登录失败，是否重试？（剩余 %1 次）").arg(maxRetries - retryCount),
                QMessageBox::Retry | QMessageBox::Cancel
            );
    
            if (reply == QMessageBox::Cancel) {
                return false;
            }
        }
    } else {
        qDebug() << "[MainWindow] 用户已登录:" << m_userAuthManager->getCurrentUsername();
    }

    // 测试模式：以管理员身份直接登录
    m_userAuthManager->setTestAdminMode();
    qDebug() << "[MainWindow] 测试模式：跳过登录，以管理员身份进入主界面";
    return true;
}

MainWindow::~MainWindow()
{
    // 先停止 DataProcessor 线程
    if (m_nav2ViewProcessor && m_nav2ViewProcessor->isRunning()) {
        m_nav2ViewProcessor->stopProcessing();
        m_nav2ViewProcessor->wait(3000);
    }

    stopAllThreads();

    // 清理预设路径进程
    if (m_waypointProcess && m_waypointProcess->state() != QProcess::NotRunning) {
        m_waypointProcess->terminate();
        if (!m_waypointProcess->waitForFinished(3000)) {
            m_waypointProcess->kill();
        }
    }

    // 智能指针自动管理资源，无需手动delete
    delete ui;
}

void MainWindow::initializeThreads()
{
    m_logStorage = std::make_unique<LogStorageEngine>();
    QString dbPath = "./logs/unified_logs.db";
    if (!m_logStorage->initialize(dbPath)) {
        qWarning() << "[MainWindow] Failed to initialize LogStorageEngine:" << m_logStorage->getLastError();
    } else {
        m_logStorage->startAutoCleanup();
    }

    m_robotStatusThread = std::make_unique<RobotStatusThread>(this);
    m_systemMonitorThread = std::make_unique<SystemMonitorThread>(this);
    m_logThread = std::make_unique<LogThread>(m_logStorage.get(), this);
    m_paramThread = std::make_unique<Nav2ParameterThread>(this);

    m_logTableModel->setStorageEngine(m_logThread->getStorageEngine());

    m_nav2ViewProcessor = std::make_unique<Nav2ViewDataProcessor>(m_mapPath, this);
    m_nav2ViewWidget = std::make_unique<Nav2ViewWidget>(this);
    m_mapCache = std::make_unique<MapCache>(10, this);
    m_navigationActionThread = std::make_unique<NavigationActionThread>(this);
    m_pathVisualizer = nullptr;
}

void MainWindow::connectSignals()
{
    // Nav2View信号连接
    connect(m_nav2ViewProcessor.get(), &Nav2ViewDataProcessor::mapLoadSucceeded,
            m_nav2ViewWidget.get(), &Nav2ViewWidget::onMapLoaded);
    connect(m_nav2ViewProcessor.get(), &Nav2ViewDataProcessor::mapLoadFailed,
            this, [this](const QString& error) {
                qWarning() << "[MainWindow]" << error;
            });
    connect(m_nav2ViewProcessor.get(), &Nav2ViewDataProcessor::renderDataReady,
            m_nav2ViewWidget.get(), &Nav2ViewWidget::onRenderDataReady,
            Qt::QueuedConnection);
    connect(m_nav2ViewWidget.get(), &Nav2ViewWidget::goalCleared,
            m_nav2ViewProcessor.get(), &Nav2ViewDataProcessor::onGoalCleared,
            Qt::QueuedConnection);

    // RobotStatusThread信号连接
    connect(m_robotStatusThread.get(), &RobotStatusThread::batteryStatusReceived,
            this, &MainWindow::onBatteryStatusReceived);
    connect(m_robotStatusThread.get(), &RobotStatusThread::positionReceived,
            this, &MainWindow::onPositionReceived);
    connect(m_robotStatusThread.get(), &RobotStatusThread::odometryReceived,
            this, &MainWindow::onOdometryReceived);
    connect(m_robotStatusThread.get(), &RobotStatusThread::systemTimeReceived,
            this, &MainWindow::onSystemTimeReceived);
    connect(m_robotStatusThread.get(), &RobotStatusThread::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);

    connect(m_robotStatusThread.get(), &RobotStatusThread::logMessage,
            m_logThread.get(), &LogThread::writeLog);

    // SystemMonitorThread信号连接
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::logMessageReceived,
            this, &MainWindow::onLogMessageReceived);
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::collisionDetected,
            this, &MainWindow::onCollisionDetected);
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::exceptionDetected,
            this, &MainWindow::onExceptionDetected);
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::behaviorTreeLogReceived,
            this, &MainWindow::onBehaviorTreeLogReceived);
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::manualInterventionReceived,
            this, &MainWindow::onManualInterventionReceived);

    // RobotStatusThread的诊断信息转发给SystemMonitorThread
    connect(m_robotStatusThread.get(), &RobotStatusThread::diagnosticsReceived,
            m_systemMonitorThread.get(), &SystemMonitorThread::onDiagnosticsReceived);

    // LogThread信号连接
    connect(m_logThread.get(), &LogThread::logFileChanged,
            this, &MainWindow::onLogFileChanged);

    // 线程状态信号连接
    connect(m_robotStatusThread.get(), &RobotStatusThread::threadStarted,
            this, &MainWindow::onThreadStarted);
    connect(m_robotStatusThread.get(), &RobotStatusThread::threadStopped,
            this, &MainWindow::onThreadStopped);
    connect(m_robotStatusThread.get(), &RobotStatusThread::threadError,
            this, &MainWindow::onThreadError);

    connect(m_systemMonitorThread.get(), &SystemMonitorThread::threadStarted,
            this, &MainWindow::onThreadStarted);
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::threadStopped,
            this, &MainWindow::onThreadStopped);
    connect(m_systemMonitorThread.get(), &SystemMonitorThread::threadError,
            this, &MainWindow::onThreadError);

    connect(m_logThread.get(), &LogThread::threadStarted,
            this, &MainWindow::onThreadStarted);
    connect(m_logThread.get(), &LogThread::threadStopped,
            this, &MainWindow::onThreadStopped);
    connect(m_logThread.get(), &LogThread::threadError,
            this, &MainWindow::onThreadError);

    // 连接过滤控件信号
    connect(ui->debugCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onFilterChanged);
    connect(ui->infoCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onFilterChanged);
    connect(ui->warningCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onFilterChanged);
    connect(ui->errorCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onFilterChanged);
    connect(ui->fatalCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onFilterChanged);
    connect(ui->highFreqCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onFilterChanged);
    connect(ui->collisionCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onFilterChanged);

    // 连接日志按钮信号
    connect(ui->clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogByLevel);
    connect(ui->pauseLogButton, &QPushButton::toggled, this, &MainWindow::onPauseLogToggled);

    // 连接历史日志相关信号
    connect(ui->queryButton, &QPushButton::clicked, this, &MainWindow::onHistoryLogQuery);
    connect(ui->todayButton, &QPushButton::clicked, this, &MainWindow::onTodayButtonClicked);
    connect(ui->yesterdayButton, &QPushButton::clicked, this, &MainWindow::onYesterdayButtonClicked);
    connect(ui->last7DaysButton, &QPushButton::clicked, this, &MainWindow::onLast7DaysButtonClicked);
    connect(ui->last30DaysButton, &QPushButton::clicked, this, &MainWindow::onLast30DaysButtonClicked);
    connect(ui->firstPageButton, &QPushButton::clicked, this, &MainWindow::onFirstPage);
    connect(ui->prevPageButton, &QPushButton::clicked, this, &MainWindow::onPrevPage);
    connect(ui->nextPageButton, &QPushButton::clicked, this, &MainWindow::onNextPage);
    connect(ui->lastPageButton, &QPushButton::clicked, this, &MainWindow::onLastPage);
    connect(ui->pageSizeComboBox, &QComboBox::currentTextChanged, this, &MainWindow::onPageSizeChanged);
    connect(ui->exportButton, &QPushButton::clicked, this, &MainWindow::onExportHistoryLogs);
    connect(ui->clearHistoryButton, &QPushButton::clicked, this, &MainWindow::onClearHistoryLogs);
    connect(ui->selectAllLevelsButton, &QPushButton::clicked, this, &MainWindow::onSelectAllLevels);
    connect(ui->deselectAllLevelsButton, &QPushButton::clicked, this, &MainWindow::onDeselectAllLevels);

    // 历史日志查询 QFutureWatcher 初始化
    m_historyLogQueryWatcher = new QFutureWatcher<LogQueryResult>(this);
    connect(m_historyLogQueryWatcher, &QFutureWatcher<LogQueryResult>::finished,
            this, &MainWindow::onHistoryLogQueryFinished);

    m_historyPageLoadWatcher = new QFutureWatcher<LogQueryResult>(this);
    connect(m_historyPageLoadWatcher, &QFutureWatcher<LogQueryResult>::finished,
            this, &MainWindow::onHistoryPageLoadFinished);

    // 连接按钮信号
    connect(ui->btnStartNavigation, &QPushButton::clicked, this, &MainWindow::onStartNavigation);
    connect(ui->btnCancelNavigation, &QPushButton::clicked, this, &MainWindow::onCancelNavigation);
    connect(ui->btnClearGoal, &QPushButton::clicked, this, &MainWindow::onClearGoal);
    connect(ui->btnRunWaypoints, &QPushButton::clicked, this, &MainWindow::onRunWaypoints);

    // 用户菜单信号连接
    connect(ui->actionUserManagement, &QAction::triggered, this, &MainWindow::onUserManagement);
    connect(ui->actionChangePassword, &QAction::triggered, this, &MainWindow::onChangePassword);
    connect(ui->actionLogout, &QAction::triggered, this, &MainWindow::onLogout);

    connect(m_navigationActionThread.get(), &NavigationActionThread::goalAccepted,
            this, &MainWindow::onGoalAccepted);
    connect(m_navigationActionThread.get(), &NavigationActionThread::goalRejected,
            this, &MainWindow::onGoalRejected);
    connect(m_navigationActionThread.get(), &NavigationActionThread::feedbackReceived,
            this, &MainWindow::onNavigationFeedback);
    connect(m_navigationActionThread.get(), &NavigationActionThread::resultReceived,
            this, &MainWindow::onNavigationResult);
    connect(m_navigationActionThread.get(), &NavigationActionThread::goalCanceled,
            this, &MainWindow::onGoalCanceled);

    // NavigationActionThread 线程状态信号
    connect(m_navigationActionThread.get(), &NavigationActionThread::threadStarted,
            this, &MainWindow::onThreadStarted);
    connect(m_navigationActionThread.get(), &NavigationActionThread::threadStopped,
            this, &MainWindow::onThreadStopped);
    connect(m_navigationActionThread.get(), &NavigationActionThread::threadError,
            this, &MainWindow::onThreadError);

    // NavigationActionThread 日志转发
    connect(m_navigationActionThread.get(), &NavigationActionThread::logMessage,
            m_logThread.get(), &LogThread::writeLog);

    // Nav2ViewWidget信号连接
    connect(m_nav2ViewWidget.get(), &Nav2ViewWidget::goalPosePreview,
            this, [this](double x, double y, double yaw) {
                m_targetX = x;
                m_targetY = y;
                m_targetYaw = yaw;
                m_hasTarget = true;
                ui->labelTargetPosition->setText(QString("(%1, %2)").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2));
                // 同步更新 processor 的渲染数据，使箭头能正确显示
                if (m_nav2ViewProcessor) {
                    QMetaObject::invokeMethod(m_nav2ViewProcessor.get(), "updateGoalPoseOnly",
                        Qt::QueuedConnection,
                        Q_ARG(double, x),
                        Q_ARG(double, y),
                        Q_ARG(double, yaw));
                }
            });

    // Nav2ParameterThread 信号连接
    connect(m_paramThread.get(), &Nav2ParameterThread::threadStarted,
            this, &MainWindow::onThreadStarted);
    connect(m_paramThread.get(), &Nav2ParameterThread::threadStopped,
            this, &MainWindow::onThreadStopped);
    connect(m_paramThread.get(), &Nav2ParameterThread::threadError,
            this, &MainWindow::onThreadError);
    connect(m_paramThread.get(), &Nav2ParameterThread::parameterRefreshed,
            this, &MainWindow::onParameterRefreshed);
    connect(m_paramThread.get(), &Nav2ParameterThread::parameterApplied,
            this, &MainWindow::onParameterApplied);
    connect(m_paramThread.get(), &Nav2ParameterThread::operationFinished,
            this, &MainWindow::onParameterOperationFinished);

    // Settings Tab 按钮连接
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshButtonClicked);
    connect(ui->applyButton, &QPushButton::clicked, this, &MainWindow::onApplyButtonClicked);
    connect(ui->resetButton, &QPushButton::clicked, this, &MainWindow::onResetButtonClicked);
    connect(ui->discardButton, &QPushButton::clicked, this, &MainWindow::onDiscardButtonClicked);

    // 参数控件值变化连接
    connect(ui->maxVelXSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->minVelXSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->maxVelThetaSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->lookaheadDistSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->localInflationRadiusSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->localCostScalingFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->globalInflationRadiusSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->globalCostScalingFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->smootherMaxVelSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->smootherMinVelSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->smootherMaxAccelSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->smootherMaxDecelSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
    connect(ui->robotRadiusSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onParameterValueChanged);
}

void MainWindow::startAllThreads()
{
    m_nav2ViewProcessor->start();

    connect(m_logThread.get(), &LogThread::threadStarted, this, [this]() {
        QTimer::singleShot(THREAD_START_DELAY_MS, m_systemMonitorThread.get(), [this]() {
            m_systemMonitorThread->start();
        });
    }, Qt::UniqueConnection);

    connect(m_systemMonitorThread.get(), &SystemMonitorThread::threadStarted, this, [this]() {
        QTimer::singleShot(THREAD_START_DELAY_MS, m_robotStatusThread.get(), [this]() {
            m_robotStatusThread->start();
        });
    }, Qt::UniqueConnection);

    connect(m_robotStatusThread.get(), &RobotStatusThread::threadStarted, this, [this]() {
        QTimer::singleShot(THREAD_START_DELAY_MS, m_paramThread.get(), [this]() {
            m_paramThread->start();
        });
    }, Qt::UniqueConnection);

    connect(m_paramThread.get(), &Nav2ParameterThread::threadStarted, this, [this]() {
        QTimer::singleShot(THREAD_START_DELAY_MS, m_navigationActionThread.get(), [this]() {
            m_navigationActionThread->start();
        });
    }, Qt::UniqueConnection);

    m_logThread->start();
}

void MainWindow::stopAllThreads()
{
    // 停止顺序与启动相反
    // 使用智能指针的bool转换操作符检查有效性

    // 先停止 NavigationActionThread
    if (m_navigationActionThread && m_navigationActionThread->isRunning()) {
        m_navigationActionThread->stopThread();
        if (!m_navigationActionThread->wait(THREAD_STOP_TIMEOUT_MS)) {
            qWarning() << "[MainWindow] 警告：导航动作线程停止超时";
        }
    }

    if (m_paramThread && m_paramThread->isRunning()) {
        m_paramThread->stopThread();
        if (!m_paramThread->wait(THREAD_STOP_TIMEOUT_MS)) {
            qWarning() << "[MainWindow] 警告：参数线程停止超时";
        }
    }

    if (m_robotStatusThread && m_robotStatusThread->isRunning()) {
        m_robotStatusThread->stopThread();
        if (!m_robotStatusThread->wait(THREAD_STOP_TIMEOUT_MS)) {
            qWarning() << "[MainWindow] 警告：机器人状态线程停止超时";
        }
    }

    if (m_systemMonitorThread && m_systemMonitorThread->isRunning()) {
        m_systemMonitorThread->stopThread();
        if (!m_systemMonitorThread->wait(THREAD_STOP_TIMEOUT_MS)) {
            qWarning() << "[MainWindow] 警告：系统监控线程停止超时";
        }
    }

    if (m_logThread && m_logThread->isRunning()) {
        m_logThread->stopThread();
        if (!m_logThread->wait(THREAD_STOP_TIMEOUT_MS)) {
            qWarning() << "[MainWindow] 警告：日志线程停止超时";
        }
    }
}

// ==================== RobotStatusThread槽函数 ====================

void MainWindow::onBatteryStatusReceived(float voltage, float percentage)
{
    ui->labelVoltage->setText(QString("%1 V").arg(voltage, 0, 'f', 2));
    ui->labelPercentage->setText(QString("%1 %").arg(percentage, 0, 'f', 1));

    QString statusText;
    QString color;

    if (voltage > 13.0f) {
        statusText = "充电中";
        color = "green";
    } else if (percentage < 20.0f) {
        statusText = "低电量警告";
        color = "red";
    } else {
        statusText = "放电中";
        color = "blue";
    }

    ui->labelBatteryStatus->setText(QString("状态：%1").arg(statusText));
    ui->labelBatteryStatus->setStyleSheet(QString("color: %1;").arg(color));

    if (percentage < 20.0f) {
        ui->labelPercentage->setStyleSheet("color: red; font-weight: bold;");
    } else {
        ui->labelPercentage->setStyleSheet("");
    }

    QString message = QString("电池状态 - 电压: %1 V, 电量: %2%").arg(voltage, 0, 'f', 2).arg(percentage, 0, 'f', 1);
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "RobotStatus");
    addLogEntry(entry);
}

void MainWindow::onPositionReceived(double x, double y, double yaw)
{
    ui->labelX->setText(QString("%1 m").arg(x, 0, 'f', 3));
    ui->labelY->setText(QString("%1 m").arg(y, 0, 'f', 3));
    
    double yaw_degrees = yaw * 180.0 / M_PI;
    ui->labelYaw->setText(QString("%1 °").arg(yaw_degrees, 0, 'f', 1));

    QString message = QString("位置信息 - X: %1, Y: %2, Yaw: %3").arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(yaw, 0, 'f', 2);
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "RobotStatus");
    addLogEntry(entry);
}

void MainWindow::onOdometryReceived(double x, double y, double yaw, double vx, double vy, double omega)
{
    if (ui->labelX->text() == "-- m") {
        ui->labelX->setText(QString("%1 m").arg(x, 0, 'f', 3));
        ui->labelY->setText(QString("%1 m").arg(y, 0, 'f', 3));

        double yaw_degrees = yaw * 180.0 / M_PI;
        ui->labelYaw->setText(QString("%1 °").arg(yaw_degrees, 0, 'f', 1));
    }

    // 更新速度显示
    ui->labelLinearVel->setText(QString("%1 m/s").arg(vx, 0, 'f', 2));
    ui->labelAngularVel->setText(QString("%1 rad/s").arg(omega, 0, 'f', 2));

    QString message = QString("里程计 - 位置(X:%1, Y:%2, Yaw:%3), 速度(vx:%4, vy:%5, omega:%6)")
        .arg(x, 0, 'f', 2).arg(y, 0, 'f', 2).arg(yaw, 0, 'f', 2)
        .arg(vx, 0, 'f', 2).arg(vy, 0, 'f', 2).arg(omega, 0, 'f', 2);
    LogEntry entry(message, LogLevel::ODOMETRY, QDateTime::currentDateTime(), "RobotStatus");
    addLogEntry(entry);
}

void MainWindow::onSystemTimeReceived(const QString& time)
{
    QStringList parts = time.split(' ');
    if (parts.size() == 2) {
        ui->labelCurrentTime->setText(parts[0] + "\n" + parts[1]);
    } else {
        ui->labelCurrentTime->setText(time);
    }
    ui->statusbar->showMessage(QString("系统时间: %1").arg(time));
}

// ==================== SystemMonitorThread槽函数 ====================

void MainWindow::onLogMessageReceived(const QString& message, int level, const QDateTime& timestamp)
{
    LogEntry entry(message, static_cast<LogLevel>(level), timestamp, "SystemMonitor");
    addLogEntry(entry);
}

void MainWindow::onCollisionDetected(const QString& message)
{
    QString logMessage = QString("碰撞检测 - %1").arg(message);
    LogEntry entry(logMessage, LogLevel::ERROR, QDateTime::currentDateTime(), "SystemMonitor");
    addLogEntry(entry);

    ui->statusbar->showMessage(QString("警告: %1").arg(message), 5000);
}

void MainWindow::onExceptionDetected(const QString& message)
{
    QString logMessage = QString("异常检测 - %1").arg(message);
    LogEntry entry(logMessage, LogLevel::WARNING, QDateTime::currentDateTime(), "SystemMonitor");
    addLogEntry(entry);

    ui->statusbar->showMessage(QString("异常: %1").arg(message), 5000);
}

void MainWindow::onBehaviorTreeLogReceived(const QString& log)
{
    QString message = QString("行为树日志 - %1").arg(log);
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "SystemMonitor");
    addLogEntry(entry);
}

// ==================== LogThread槽函数 ====================

void MainWindow::onLogFileChanged(const QString& filePath)
{
    QString message = QString("日志文件变更 - 新文件: %1").arg(filePath);
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "LogThread");
    addLogEntry(entry);
}

// ==================== 线程状态槽函数 ====================

void MainWindow::onConnectionStateChanged(bool connected)
{
    QString status = connected ? "已连接" : "已断开";
    ui->statusbar->showMessage(QString("ROS连接状态: %1").arg(status), 3000);

    QString message = QString("连接状态变更 - %1").arg(status);
    LogLevel logLevel = connected ? LogLevel::INFO : LogLevel::WARNING;
    LogEntry entry(message, logLevel, QDateTime::currentDateTime(), "System");
    addLogEntry(entry);
}

void MainWindow::onThreadStarted(const QString& threadName)
{
    QString message = QString("线程启动 - %1").arg(threadName);
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "System");
    addLogEntry(entry);
}

void MainWindow::onThreadStopped(const QString& threadName)
{
    QString message = QString("线程停止 - %1").arg(threadName);
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "System");
    addLogEntry(entry);
}

void MainWindow::onThreadError(const QString& error)
{
    QString message = QString("线程错误 - %1").arg(error);
    // LogEntry entry(message, LOG_ERROR, QDateTime::currentDateTime(), "System", "Thread");
    // m_logTableModel->addLogEntry(entry);

    ui->statusbar->showMessage(QString("错误: %1").arg(error), 5000);

    QMessageBox::critical(this, "线程错误", QString("发生线程错误:\n\n%1\n\n请检查日志获取详细信息。").arg(error));
}

void MainWindow::onFilterChanged()
{
    refreshLogDisplay(false);  // 手动过滤不自动滚动
}

void MainWindow::refreshLogDisplay(bool autoScroll)
{
    // 获取当前勾选的日志级别
    QSet<int> enabledLevels;
    if (ui->debugCheckBox->isChecked()) enabledLevels << static_cast<int>(LogLevel::DEBUG);
    if (ui->infoCheckBox->isChecked()) enabledLevels << static_cast<int>(LogLevel::INFO);
    if (ui->warningCheckBox->isChecked()) enabledLevels << static_cast<int>(LogLevel::WARNING);
    if (ui->errorCheckBox->isChecked()) enabledLevels << static_cast<int>(LogLevel::ERROR);
    if (ui->fatalCheckBox->isChecked()) enabledLevels << static_cast<int>(LogLevel::FATAL);
    if (ui->highFreqCheckBox->isChecked()) enabledLevels << static_cast<int>(LogLevel::HIGHFREQ);
    if (ui->collisionCheckBox->isChecked()) enabledLevels << static_cast<int>(LogLevel::COLLISION);

    // 使用LogFilterProxyModel的过滤功能，避免重建模型
    m_logFilterProxyModel->setLogLevelFilter(enabledLevels);

    if (autoScroll) {
        ui->logTableView->scrollToBottom();
    }
}

void MainWindow::onClearLogByLevel()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    // 获取当前勾选的日志级别
    QSet<int> levelsToClear;
    if (ui->debugCheckBox->isChecked()) levelsToClear << static_cast<int>(LogLevel::DEBUG);
    if (ui->infoCheckBox->isChecked()) levelsToClear << static_cast<int>(LogLevel::INFO);
    if (ui->warningCheckBox->isChecked()) levelsToClear << static_cast<int>(LogLevel::WARNING);
    if (ui->errorCheckBox->isChecked()) levelsToClear << static_cast<int>(LogLevel::ERROR);
    if (ui->fatalCheckBox->isChecked()) levelsToClear << static_cast<int>(LogLevel::FATAL);
    if (ui->highFreqCheckBox->isChecked()) levelsToClear << static_cast<int>(LogLevel::HIGHFREQ);
    if (ui->collisionCheckBox->isChecked()) levelsToClear << static_cast<int>(LogLevel::COLLISION);

    // 从内存中清除对应级别的日志（仅影响显示，不影响数据库和文件中的日志）
    int clearedCount = 0;

    // 清除日志缓存
    for (auto it = m_allLogs.begin(); it != m_allLogs.end(); ) {
        if (levelsToClear.contains(static_cast<int>(it->level))) {
            it = m_allLogs.erase(it);
            ++clearedCount;
        } else {
            ++it;
        }
    }

    // 刷新模型显示
    m_logTableModel->clearLogs();
    for (const auto& entry : m_allLogs) {
        m_logTableModel->addLogEntry(entry);
    }

    ui->statusbar->showMessage(QString("已清除 %1 条勾选类型的日志（仅显示）").arg(clearedCount), 3000);
}

void MainWindow::onPauseLogToggled(bool checked)
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        // 恢复按钮状态
        ui->pauseLogButton->setChecked(!checked);
        return;
    }

    m_logPaused = checked;
    if (checked) {
        ui->pauseLogButton->setText("继续接收");
        ui->statusbar->showMessage("日志接收已暂停", 3000);
    } else {
        ui->pauseLogButton->setText("暂停接收");
        ui->statusbar->showMessage("日志接收已恢复", 3000);
    }
}

void MainWindow::onStartNavigation()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    if (!m_hasTarget) {
        QMessageBox::warning(this, tr("提示"), tr("请设置导航目标"));
        return;
    }

    // 检查指针有效性
    if (!m_nav2ViewWidget) {
        qCritical() << "[MainWindow] 错误：Nav2ViewWidget 未初始化";
        QMessageBox::critical(this, tr("错误"), tr("导航视图组件未初始化"));
        return;
    }

    if (!m_navigationActionThread) {
        qCritical() << "[MainWindow] 错误：NavigationActionThread 未初始化";
        QMessageBox::critical(this, tr("错误"), tr("导航动作线程未初始化"));
        return;
    }

    // 重置初始距离，将在第一次反馈时使用 Nav2 返回的距离设置
    m_initialDistance = 0.0;
    qInfo() << "[MainWindow] 重置初始距离，等待第一次反馈";

    // 重置人工干预状态
    m_lastRecoveries = 0;
    updateInterventionStatus(Normal);

    // 通过 NavigationActionThread 发送导航目标（使用 Action 而不是话题）
    m_navigationActionThread->sendGoalToPose(m_targetX, m_targetY, m_targetYaw);

    ui->labelNavigationStatus->setText(tr("导航中"));
    ui->labelNavigationStatus->setStyleSheet("color: blue;");
}

void MainWindow::onCancelNavigation()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    if (!m_navigationActionThread) {
        qCritical() << "[MainWindow] 错误：NavigationActionThread 未初始化";
        QMessageBox::critical(this, tr("错误"), tr("导航动作线程未初始化"));
        return;
    }

    // 移除 isNavigating() 检查，直接尝试取消
    // 原因：客户端状态可能与服务端不同步
    bool success = m_navigationActionThread->cancelCurrentGoal();
    if (!success) {
        QMessageBox::information(this, tr("提示"), tr("无导航任务或取消失败"));
        return;
    }

    // 取消导航时保留目标点显示
    ui->labelNavigationStatus->setText(tr("已取消"));
    ui->labelNavigationStatus->setStyleSheet("color: orange;");
}

void MainWindow::onClearGoal()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    m_hasTarget = false;
    m_targetX = 0.0;
    m_targetY = 0.0;
    m_targetYaw = 0.0;
    m_initialDistance = 0.0;

    // 清除 Nav2ViewWidget 的目标状态，检查指针有效性
    if (m_nav2ViewWidget) {
        m_nav2ViewWidget->clearGoal();
    }

    ui->labelTargetPosition->setText(tr("未设置"));
    ui->labelNavigationStatus->setText(tr("空闲"));
    ui->labelNavigationStatus->setStyleSheet("color: gray;");
    ui->labelDistanceRemaining->setText("-- m");
    ui->labelNavigationTime->setText("-- s");
    ui->labelRecoveries->setText("0");
    ui->navigationProgressBar->setValue(0);
    ui->labelEstimatedTime->setText("--:--");
}

void MainWindow::onNavigationFeedback(double distanceRemaining, double navigationTime, int recoveries, double estimatedTimeRemaining)
{
    // 检测恢复行为执行中
    if (recoveries > m_lastRecoveries) {
        if (m_interventionStatus != Intervention) {
            updateInterventionStatus(Recovering);
        }
    }
    m_lastRecoveries = recoveries;

    // 更新剩余距离
    ui->labelDistanceRemaining->setText(QString::number(distanceRemaining, 'f', 2) + " m");

    // 更新导航时间
    int totalSeconds = static_cast<int>(navigationTime);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    QString timeStr = QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
    ui->labelNavigationTime->setText(timeStr);

    // 更新预计剩余时间
    int estSeconds = static_cast<int>(estimatedTimeRemaining);
    int estMinutes = estSeconds / 60;
    int estSec = estSeconds % 60;
    QString estTimeStr = QString("%1:%2")
        .arg(estMinutes, 2, 10, QChar('0'))
        .arg(estSec, 2, 10, QChar('0'));
    ui->labelEstimatedTime->setText(estTimeStr);

    // 更新恢复次数
    ui->labelRecoveries->setText(QString::number(recoveries));

    // 更新导航进度
    // 设置初始距离：需要距离大于最小阈值（避免使用上一个任务的旧反馈）
    const double MIN_INITIAL_DISTANCE = 0.5;  // 最小初始距离阈值（米）
    if (m_initialDistance <= 0.0 && distanceRemaining > MIN_INITIAL_DISTANCE) {
        m_initialDistance = distanceRemaining;
        qInfo() << "[MainWindow] 设置初始距离为第一次反馈值:" << m_initialDistance;
    } else if (m_initialDistance <= 0.0 && distanceRemaining > 0.0) {
        qDebug() << "[MainWindow] 忽略过小的反馈距离（可能是旧任务反馈）: distanceRemaining=" << distanceRemaining;
    } else if (m_initialDistance <= 0.0) {
        qDebug() << "[MainWindow] 跳过设置初始距离: m_initialDistance=" << m_initialDistance << ", distanceRemaining=" << distanceRemaining;
    }

    if (m_initialDistance > 0.0 && distanceRemaining >= 0.0) {
        static int progressLogCount = 0;
        progressLogCount++;
        double progress = ((m_initialDistance - distanceRemaining) / m_initialDistance) * 100.0;
        progress = std::max(0.0, std::min(100.0, progress));
        ui->navigationProgressBar->setValue(static_cast<int>(progress));
        if (progressLogCount % 50 == 1) {
            qDebug() << "[MainWindow] 导航进度:" << progress << "%, 剩余距离:" << distanceRemaining;
        }
    }

    // 日志记录
    QString message = QString("导航反馈 - 剩余距离: %1m, 导航时间: %2s, 恢复次数: %3, 预计剩余: %4")
        .arg(distanceRemaining, 0, 'f', 2)
        .arg(navigationTime, 0, 'f', 1)
        .arg(recoveries)
        .arg(estTimeStr);
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "Navigation");
    addLogEntry(entry);
}

void MainWindow::onNavigationResult(bool success, const QString& message)
{
    // 检查指针有效性
    if (!m_navigationActionThread) {
        qCritical() << "[MainWindow] 错误：NavigationActionThread 未初始化";
        return;
    }

    if (success) {
        QMessageBox::information(this, tr("导航完成"), tr("导航成功到达目标点"));
        ui->labelNavigationStatus->setText(tr("导航成功"));
        ui->labelNavigationStatus->setStyleSheet("color: green;");
        ui->navigationProgressBar->setValue(100);
    } else {
        QMessageBox::warning(this, tr("导航失败"), message);
        ui->labelNavigationStatus->setText(tr("导航失败"));
        ui->labelNavigationStatus->setStyleSheet("color: red;");
    }

    // 重置初始距离
    m_initialDistance = 0.0;

    // 重置人工干预状态
    m_lastRecoveries = 0;
    updateInterventionStatus(Normal);
    if (m_interventionDialog) {
        m_interventionDialog->close();
    }

    QString logMessage = QString("导航结果 - %1: %2").arg(success ? tr("成功") : tr("失败")).arg(message);
    LogLevel logLevel = success ? LogLevel::INFO : LogLevel::WARNING;
    LogEntry entry(logMessage, logLevel, QDateTime::currentDateTime(), "Navigation");
    addLogEntry(entry);
}

void MainWindow::onGoalAccepted()
{
    qInfo() << "[MainWindow] onGoalAccepted 被调用，重置初始距离为 0";
    ui->labelNavigationStatus->setText("导航中");
    ui->labelNavigationStatus->setStyleSheet("color: blue;");

    // 重置初始距离，将在第一次反馈时设置
    m_initialDistance = 0.0;

    QString message = "导航目标已被接受，开始导航";
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "Navigation");
    addLogEntry(entry);
    ui->statusbar->showMessage(message, 3000);
}

void MainWindow::onGoalRejected(const QString& reason)
{
    QMessageBox::warning(this, "目标被拒绝", reason);
    ui->labelNavigationStatus->setText("目标被拒绝");
    ui->labelNavigationStatus->setStyleSheet("color: orange;");

    QString message = QString("导航目标被拒绝 - 原因: %1").arg(reason);
    LogEntry entry(message, LogLevel::WARNING, QDateTime::currentDateTime(), "Navigation");
    addLogEntry(entry);
}

void MainWindow::onGoalCanceled()
{
    QMessageBox::information(this, "导航已取消", "导航目标已取消");
    ui->labelNavigationStatus->setText("已取消");
    ui->labelNavigationStatus->setStyleSheet("color: gray;");

    // 重置人工干预状态
    m_lastRecoveries = 0;
    updateInterventionStatus(Normal);
    if (m_interventionDialog) {
        m_interventionDialog->close();
    }

    QString message = "导航目标已取消";
    LogEntry entry(message, LogLevel::INFO, QDateTime::currentDateTime(), "Navigation");
    addLogEntry(entry);
}

void MainWindow::updateCurrentTime()
{
    QDateTime now = QDateTime::currentDateTime();
    QString dateStr = now.toString("yyyy-MM-dd");
    QString timeStr = now.toString("HH:mm:ss");
    ui->labelCurrentTime->setText(dateStr + "\n" + timeStr);

    qint64 uptimeSec = m_startTime.secsTo(QDateTime::currentDateTime());
    int hours = uptimeSec / 3600;
    int minutes = (uptimeSec % 3600) / 60;
    int seconds = uptimeSec % 60;

    QString uptimeStr = QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));

    ui->labelUptime->setText(uptimeStr);
}

// ==================== 用户权限管理槽函数 ====================

void MainWindow::onLoginSuccess(const User& user)
{
    updateUIBasedOnPermission();
}

void MainWindow::onLoginFailed(const QString& reason)
{
}

void MainWindow::onLogout()
{
    try {
        // 检查指针有效性
        if (!m_userAuthManager) {
            qCritical() << "[MainWindow] 错误：UserAuthManager 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("用户认证管理器未初始化"));
            return;
        }

        if (!m_loginDialog) {
            qCritical() << "[MainWindow] 错误：LoginDialog 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("登录对话框未初始化"));
            return;
        }

        // 检查是否有正在进行的导航操作
        if (m_hasTarget && m_navigationActionThread && m_navigationActionThread->isNavigating()) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this,
                tr("确认登出"),
                tr("导航正在进行中，确定要登出吗？"),
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                return;
            }
            // 取消当前导航目标
            onClearGoal();
        }

        m_userAuthManager->logout();

        // 清理当前状态
        m_hasTarget = false;
        m_targetX = 0.0;
        m_targetY = 0.0;
        m_targetYaw = 0.0;

        // 隐藏主窗口
        this->hide();

        // 重新登录循环，添加最大重试次数限制
        int maxRetries = 3;
        int retryCount = 0;

        while (retryCount < maxRetries) {
            // 解除父子关系，使对话框成为独立窗口
            m_loginDialog->setParent(nullptr, Qt::Window);
            // 确保对话框启用
            m_loginDialog->setEnabled(true);

            m_loginDialog->exec();

            if (m_userAuthManager->isLoggedIn()) {
                // 登录成功，恢复父子关系
                m_loginDialog->setParent(this);
                m_loginDialog->hide();  // 隐藏对话框
                // 显示并激活主窗口
                this->show();
                this->activateWindow();
                this->raise();
                break;
            }

            retryCount++;

            if (retryCount >= maxRetries) {
                qCritical() << "[MainWindow] 错误：登录重试次数超过限制，程序将退出";
                // 恢复父子关系
                m_loginDialog->setParent(this);
                QMessageBox::critical(nullptr, tr("登录失败"),
                    tr("登录失败次数过多，程序将退出"));
                QApplication::quit();
                return;
            }

            QMessageBox::StandardButton reply = QMessageBox::question(
                nullptr,
                tr("登录失败"),
                tr("登录失败，是否重试？（剩余 %1 次）").arg(maxRetries - retryCount),
                QMessageBox::Retry | QMessageBox::Cancel
            );

            if (reply == QMessageBox::Cancel) {
                // 恢复父子关系
                m_loginDialog->setParent(this);
                this->close();
                return;
            }
        }

        updateUIBasedOnPermission();
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onLogout 发生异常:" << e.what();
        QMessageBox::critical(this, tr("错误"), 
            tr("登出过程中发生错误:\n%1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MainWindow] onLogout 发生未知异常";
        QMessageBox::critical(this, tr("错误"), tr("登出过程中发生未知错误"));
    }
}

void MainWindow::onUserManagement()
{
    try {
        // 检查指针有效性
        if (!m_userAuthManager) {
            qCritical() << "[MainWindow] 错误：UserAuthManager 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("用户认证管理器未初始化"));
            return;
        }

        if (!m_userAuthManager->canAdmin()) {
            QMessageBox::warning(this, tr("权限不足"), tr("您没有权限访问用户管理功能"));
            return;
        }

        UserManagementDialog dialog(m_userAuthManager.get(), this);
        dialog.exec();
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onUserManagement 发生异常:" << e.what();
        QMessageBox::critical(this, tr("错误"), 
            tr("打开用户管理对话框时发生错误:\n%1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MainWindow] onUserManagement 发生未知异常";
        QMessageBox::critical(this, tr("错误"), tr("打开用户管理对话框时发生未知错误"));
    }
}

void MainWindow::onChangePassword()
{
    try {
        // 检查指针有效性
        if (!m_userAuthManager) {
            qCritical() << "[MainWindow] 错误：UserAuthManager 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("用户认证管理器未初始化"));
            return;
        }

        ChangePasswordDialog dialog(ChangePasswordDialog::Mode::SelfChange, m_userAuthManager.get(), "", this);
        dialog.exec();
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onChangePassword 发生异常:" << e.what();
        QMessageBox::critical(this, tr("错误"), 
            tr("打开修改密码对话框时发生错误:\n%1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MainWindow] onChangePassword 发生未知异常";
        QMessageBox::critical(this, tr("错误"), tr("打开修改密码对话框时发生未知错误"));
    }
}

void MainWindow::updateUIBasedOnPermission()
{
    try {
        // 检查指针有效性
        if (!m_userAuthManager) {
            qCritical() << "[MainWindow] 错误：UserAuthManager 未初始化";
            return;
        }

        UserPermission permission = m_userAuthManager->getCurrentPermission();

        bool canOperate = m_userAuthManager->canOperate();
        bool canAdmin = m_userAuthManager->canAdmin();

        ui->btnStartNavigation->setEnabled(canOperate);
        ui->btnCancelNavigation->setEnabled(canOperate);
        ui->btnClearGoal->setEnabled(canOperate);

        // 机器人控制按钮
        ui->btnRobotChassis->setEnabled(canOperate);
        ui->btnLidar->setEnabled(canOperate);
        ui->btnNav2->setEnabled(canOperate);
        ui->btnKeyboardControl->setEnabled(canOperate);
        ui->btnJoystick->setEnabled(canOperate);
        ui->btnRunWaypoints->setEnabled(canOperate);

        // 参数控制按钮
        ui->refreshButton->setEnabled(canOperate);
        ui->applyButton->setEnabled(canOperate);
        ui->resetButton->setEnabled(canOperate);
        ui->discardButton->setEnabled(canOperate);

        // 参数 SpinBox 只读控制（允许查看但禁止编辑）
        QDoubleSpinBox* paramSpinBoxes[] = {
            ui->maxVelXSpinBox, ui->minVelXSpinBox, ui->maxVelThetaSpinBox,
            ui->localInflationRadiusSpinBox, ui->localCostScalingFactorSpinBox,
            ui->globalInflationRadiusSpinBox, ui->globalCostScalingFactorSpinBox,
            ui->smootherMaxVelSpinBox, ui->smootherMinVelSpinBox,
            ui->smootherMaxAccelSpinBox, ui->smootherMaxDecelSpinBox,
            ui->lookaheadDistSpinBox, ui->robotRadiusSpinBox
        };
        for (auto* spinBox : paramSpinBoxes) {
            spinBox->setReadOnly(!canOperate);
            spinBox->setStyleSheet(canOperate ? "" : "QDoubleSpinBox { background-color: #f0f0f0; }");
        }

        // 日志控制按钮
        ui->clearLogButton->setEnabled(canOperate);
        ui->pauseLogButton->setEnabled(canOperate);

        // 历史日志控件权限
        bool canView = m_userAuthManager->canView();
        ui->queryButton->setEnabled(canView);
        ui->exportButton->setEnabled(canOperate);
        ui->clearHistoryButton->setEnabled(canAdmin);
        ui->clearHistoryButton->setVisible(canAdmin);

        // 同步权限到 Nav2ViewWidget
        if (m_nav2ViewWidget) {
            m_nav2ViewWidget->setOperatePermission(canOperate);
        }

        QString permissionText;
        switch (permission) {
            case UserPermission::VIEWER:
                permissionText = tr("查看者");
                break;
            case UserPermission::OPERATOR:
                permissionText = tr("操作员");
                break;
            case UserPermission::ADMIN:
                permissionText = tr("管理员");
                break;
        }

        QString username = m_userAuthManager->getCurrentUsername();
        if (username.isEmpty()) {
            username = tr("未登录");
        }
        
        QString statusText = QString(tr("当前用户: %1 (%2)")).arg(username).arg(permissionText);
        ui->statusbar->showMessage(statusText, 0);

        qDebug() << "[MainWindow] UI updated based on permission:" << permissionText;
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] updateUIBasedOnPermission 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[MainWindow] updateUIBasedOnPermission 发生未知异常";
    }
}

// ==================== Nav2ParameterThread 槽函数 ====================

void MainWindow::onRefreshButtonClicked()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    try {
        // 检查指针有效性
        if (!m_paramThread) {
            qCritical() << "[MainWindow] 错误：Nav2ParameterThread 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("参数线程未初始化"));
            return;
        }

        m_paramThread->requestRefresh();
        ui->statusLabel->setText(tr("正在刷新参数..."));
        ui->refreshButton->setEnabled(false);
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onRefreshButtonClicked 发生异常:" << e.what();
        ui->refreshButton->setEnabled(true);  // 异常时恢复按钮
        QMessageBox::critical(this, tr("错误"), tr("刷新参数时发生错误:\n%1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MainWindow] onRefreshButtonClicked 发生未知异常";
        ui->refreshButton->setEnabled(true);  // 异常时恢复按钮
        QMessageBox::critical(this, tr("错误"), tr("刷新参数时发生未知错误"));
    }
}

void MainWindow::onApplyButtonClicked()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    try {
        // 检查指针有效性
        if (!m_paramThread) {
            qCritical() << "[MainWindow] 错误：Nav2ParameterThread 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("参数线程未初始化"));
            return;
        }

        if (!m_paramThread->hasPendingChanges()) {
            QMessageBox::information(this, tr("提示"), tr("没有待应用的更改"));
            return;
        }

        auto reply = QMessageBox::question(this, tr("确认应用"),
            tr("确定要应用修改的参数到 ROS2 吗？"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_paramThread->requestApply();
            ui->statusLabel->setText(tr("正在应用参数..."));
            ui->applyButton->setEnabled(false);
        }
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onApplyButtonClicked 发生异常:" << e.what();
        ui->applyButton->setEnabled(true);  // 异常时恢复按钮
        QMessageBox::critical(this, tr("错误"), tr("应用参数时发生错误:\n%1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MainWindow] onApplyButtonClicked 发生未知异常";
        ui->applyButton->setEnabled(true);  // 异常时恢复按钮
        QMessageBox::critical(this, tr("错误"), tr("应用参数时发生未知错误"));
    }
}

void MainWindow::onResetButtonClicked()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    try {
        // 检查指针有效性
        if (!m_paramThread) {
            qCritical() << "[MainWindow] 错误：Nav2ParameterThread 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("参数线程未初始化"));
            return;
        }

        auto reply = QMessageBox::question(this, tr("确认重置"),
            tr("确定要将所有参数重置为默认值吗？\n重置后需要点击\"应用更改\"按钮。"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_paramThread->requestReset();
        }
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onResetButtonClicked 发生异常:" << e.what();
        QMessageBox::critical(this, tr("错误"), tr("重置参数时发生错误:\n%1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MainWindow] onResetButtonClicked 发生未知异常";
        QMessageBox::critical(this, tr("错误"), tr("重置参数时发生未知错误"));
    }
}

void MainWindow::onDiscardButtonClicked()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    try {
        // 检查指针有效性
        if (!m_paramThread) {
            qCritical() << "[MainWindow] 错误：Nav2ParameterThread 未初始化";
            QMessageBox::critical(this, tr("错误"), tr("参数线程未初始化"));
            return;
        }

        auto reply = QMessageBox::question(this, tr("确认放弃"),
            tr("确定要放弃所有未应用的修改吗？"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_paramThread->requestDiscard();
        }
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onDiscardButtonClicked 发生异常:" << e.what();
        QMessageBox::critical(this, tr("错误"), tr("放弃修改时发生错误:\n%1").arg(e.what()));
    } catch (...) {
        qCritical() << "[MainWindow] onDiscardButtonClicked 发生未知异常";
        QMessageBox::critical(this, tr("错误"), tr("放弃修改时发生未知错误"));
    }
}

void MainWindow::onParameterRefreshed(bool success, const QString& message)
{
    // 无论成功或失败，都要恢复按钮状态
    ui->statusLabel->setText(message);
    ui->refreshButton->setEnabled(true);

    try {
        // 无论成功或失败，都重置所有参数控件的颜色为默认
        if (m_paramThread) {
            auto params = m_paramThread->getAllParams();
            for (auto it = params.begin(); it != params.end(); ++it) {
                setParamSpinBoxColor(it.key(), Nav2ParameterThread::ParamStatus::Default);
                // 只有成功时才更新参数值
                if (success) {
                    updateParameterValue(it.key(), it.value().currentValue);
                }
            }
        }

        if (success) {
            ui->statusLabel->setStyleSheet("color: green;");
        } else {
            ui->statusLabel->setStyleSheet("color: red;");
        }

        QTimer::singleShot(3000, this, [this]() {
            ui->statusLabel->setText(tr("就绪"));
            ui->statusLabel->setStyleSheet("");
        });
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onParameterRefreshed 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[MainWindow] onParameterRefreshed 发生未知异常";
    }
}

void MainWindow::onParameterApplied(bool success, const QString& message, const QStringList& appliedKeys, const QStringList& failedKeys)
{
    // 无论成功或失败，都要恢复按钮状态
    ui->statusLabel->setText(message);
    ui->applyButton->setEnabled(true);

    try {
        if (success) {
            ui->statusLabel->setStyleSheet("color: green;");
            // 更新已应用的参数控件显示并设置绿色
            if (m_paramThread) {
                for (const QString& key : appliedKeys) {
                    Nav2ParameterThread::ParamInfo info;
                    if (m_paramThread->getParamInfo(key, info)) {
                        updateParameterValue(key, info.currentValue);
                        setParamSpinBoxColor(key, Nav2ParameterThread::ParamStatus::Success);
                    }
                }
            }
        } else {
            ui->statusLabel->setStyleSheet("color: red;");
            // 为失败的参数设置红色
            if (m_paramThread) {
                for (const QString& key : failedKeys) {
                    setParamSpinBoxColor(key, Nav2ParameterThread::ParamStatus::Failed);
                }
                // 为成功的参数也设置绿色
                for (const QString& key : appliedKeys) {
                    setParamSpinBoxColor(key, Nav2ParameterThread::ParamStatus::Success);
                }
            }
        }

        QTimer::singleShot(3000, this, [this]() {
            ui->statusLabel->setText(tr("就绪"));
            ui->statusLabel->setStyleSheet("");
            // 3秒后重置所有颜色为默认
            if (m_paramThread) {
                auto params = m_paramThread->getAllParams();
                for (auto it = params.begin(); it != params.end(); ++it) {
                    setParamSpinBoxColor(it.key(), Nav2ParameterThread::ParamStatus::Default);
                }
            }
        });
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onParameterApplied 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[MainWindow] onParameterApplied 发生未知异常";
    }
}

void MainWindow::onParameterOperationFinished(const QString& operation, bool success, const QString& message)
{
    try {
        ui->statusLabel->setText(message);

        if (success) {
            ui->statusLabel->setStyleSheet("color: green;");
            if (operation == "Reset" && m_paramThread) {
                // 重置后更新显示为 pendingValue
                auto params = m_paramThread->getAllParams();
                for (auto it = params.begin(); it != params.end(); ++it) {
                    updateParameterValue(it.key(), it.value().pendingValue);
                }
            }
        } else {
            ui->statusLabel->setStyleSheet("color: red;");
        }

        QTimer::singleShot(3000, this, [this]() {
            ui->statusLabel->setText(tr("就绪"));
            ui->statusLabel->setStyleSheet("");
        });
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onParameterOperationFinished 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[MainWindow] onParameterOperationFinished 发生未知异常";
    }
}

void MainWindow::onParameterValueChanged(double value)
{
    try {
        QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(sender());
        if (!spinBox) return;

        // 权限检查（静默拒绝，不弹窗）
        if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
            // 恢复为原值
            QString key = spinBox->objectName();
            if (m_paramThread) {
                Nav2ParameterThread::ParamInfo info;
                if (m_paramThread->getParamInfo(key, info)) {
                    spinBox->blockSignals(true);
                    spinBox->setValue(info.currentValue.toDouble());
                    spinBox->blockSignals(false);
                }
            }
            return;
        }

        // 检查指针有效性
        if (!m_paramThread) {
            qWarning() << "[MainWindow] 警告：Nav2ParameterThread 未初始化";
            return;
        }

        QString key = spinBox->objectName();
        // 控件名直接作为参数 key（不需要转换）
        m_paramThread->setPendingValue(key, value);
        // 设置待应用颜色（蓝色）
        setParamSpinBoxColor(key, Nav2ParameterThread::ParamStatus::Pending);
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] onParameterValueChanged 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[MainWindow] onParameterValueChanged 发生未知异常";
    }
}

void MainWindow::updateParameterValue(const QString& key, const QVariant& value)
{
    try {
        // key 本身就是控件名（如 "robotRadiusSpinBox"）
        QDoubleSpinBox* spinBox = findChild<QDoubleSpinBox*>(key);
        if (spinBox) {
            spinBox->setValue(value.toDouble());
        }
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] updateParameterValue 发生异常:" << e.what();
    } catch (...) {
        qCritical() << "[MainWindow] updateParameterValue 发生未知异常";
    }
}

void MainWindow::addLogEntry(const LogEntry& entry)
{
    // 如果暂停接收日志，直接返回
    if (m_logPaused) {
        return;
    }

    // 里程计日志：不显示在 UI 中，直接返回（由 LogThread 负责存储到独立数据库表）
    if (entry.level == LogLevel::ODOMETRY) {
        return;
    }

    // 写入日志线程（存储到文件和数据库）
    if (m_logThread) {
        m_logThread->writeLogEntry(entry);
    }

    // 添加到内存缓存 (最大 10000 条，用于日志管理操作)
    m_allLogs.append(entry);

    if (m_allLogs.size() > MAX_ALL_LOGS_SIZE) {
        int removeCount = m_allLogs.size() - MAX_ALL_LOGS_SIZE;
        for (int i = 0; i < removeCount; ++i) {
            m_allLogs.removeFirst();  // FIFO 删除旧日志
        }
    }

    // 添加到 UI 显示层 (LogTableModel 内部最大 1000 条，保持轻量避免卡顿)
    m_logTableModel->addLogEntry(entry);
    ui->logTableView->scrollToBottom();
}

QString MainWindow::getMapPathFromRosParam(rclcpp::Node::SharedPtr node)
{
    if (!node) return QString();
    node->declare_parameter("map_yaml_path", "");
    std::string map_path = node->get_parameter("map_yaml_path").as_string();
    if (map_path.empty()) {
        // 回退到默认路径
        return QString::fromStdString(ament_index_cpp::get_package_share_directory("robo_nav2_hmi")) + "/map/DisplaySimulationMap.yaml";
    }
    return QString::fromStdString(map_path);
}

// ==================== 历史日志槽函数 ====================

void MainWindow::onHistoryLogQuery()
{
    if (!m_userAuthManager || !m_userAuthManager->canView()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要查看者或更高权限才能查询历史日志。"));
        return;
    }

    QDateTime startTime = ui->startDateTimeEdit->dateTime();
    QDateTime endTime = ui->endDateTimeEdit->dateTime();

    if (startTime > endTime) {
        QMessageBox::warning(this, tr("参数错误"), tr("起始时间不能晚于结束时间"));
        return;
    }

    QSet<int> levels = getSelectedHistoryLogLevels();
    if (levels.isEmpty()) {
        QMessageBox::warning(this, tr("参数错误"), tr("请至少选择一个日志级别"));
        return;
    }

    QString source = ui->sourceComboBox->currentText();
    if (source == "全部") source.clear();
    QString keyword = ui->keywordLineEdit->text();
    int pageSize = ui->pageSizeComboBox->currentText().toInt();

    m_lastQueryStartTime = startTime;
    m_lastQueryEndTime = endTime;
    m_lastQuerySelectedLevels = levels;
    m_lastQuerySource = source;
    m_lastQueryKeyword = keyword;
    m_hasValidQuery = true;

    ui->queryButton->setEnabled(false);
    ui->queryButton->setText(tr("查询中..."));

    bool includeHighFreq = levels.contains(static_cast<int>(LogLevel::HIGHFREQ));
    m_lastQueryIncludeHighFreq = includeHighFreq;

    LogQueryParams params;
    params.dbPath = m_logStorage->getDbPath();
    params.startTime = startTime;
    params.endTime = endTime;
    params.selectedLevels = levels.values().toVector();
    params.source = source;
    params.keyword = keyword;
    params.limit = pageSize;
    params.offset = 0;
    params.includeHighFreq = includeHighFreq;

    QFuture<LogQueryResult> future = QtConcurrent::run([params]() {
        return LogQueryTask::execute(params);
    });

    m_historyLogQueryWatcher->setFuture(future);
}

void MainWindow::onHistoryLogQueryFinished()
{
    LogQueryResult result = m_historyLogQueryWatcher->result();

    if (!result.success) {
        QMessageBox::critical(this, tr("查询失败"), tr("查询历史日志失败:\n%1").arg(result.errorMessage));
        ui->queryButton->setEnabled(true);
        ui->queryButton->setText(tr("查询"));
        return;
    }

    int totalCount = result.totalCount;

    m_historyLogTableModel->setQueryResults(result.results);
    m_historyLogTableModel->setPaginationInfo(totalCount, 1, ui->pageSizeComboBox->currentText().toInt());

    updateHistoryLogStats();

    ui->queryButton->setEnabled(true);
    ui->queryButton->setText(tr("查询"));

    qDebug() << "[MainWindow] 历史日志查询完成，找到" << totalCount << "条记录";
}

void MainWindow::onHistoryLogQueryFailed(const QString& error)
{
    QMessageBox::critical(this, tr("查询失败"), tr("查询历史日志失败:\n%1").arg(error));

    ui->queryButton->setEnabled(true);
    ui->queryButton->setText(tr("查询"));

    qWarning() << "[MainWindow] 历史日志查询失败:" << error;
}

void MainWindow::onHistoryPageLoadFinished()
{
    LogQueryResult result = m_historyPageLoadWatcher->result();

    if (result.success) {
        m_historyLogTableModel->setQueryResults(result.results);
        updateHistoryLogStats();
        qDebug() << "[MainWindow] 历史日志分页加载完成，当前页" << m_historyLogTableModel->getCurrentPage();
    } else {
        qWarning() << "[MainWindow] 分页加载失败:" << result.errorMessage;
    }
}

void MainWindow::loadHistoryPage(int page)
{
    if (!m_hasValidQuery) {
        QMessageBox::information(this, tr("提示"), tr("请先执行查询操作"));
        return;
    }

    int totalPages = m_historyLogTableModel->getTotalPages();
    if (page < 1 || page > totalPages) {
        return;
    }

    int pageSize = ui->pageSizeComboBox->currentText().toInt();
    int offset = (page - 1) * pageSize;

    LogQueryParams params;
    params.dbPath = m_logStorage->getDbPath();
    params.startTime = m_lastQueryStartTime;
    params.endTime = m_lastQueryEndTime;
    params.selectedLevels = m_lastQuerySelectedLevels.values().toVector();
    params.source = m_lastQuerySource;
    params.keyword = m_lastQueryKeyword;
    params.limit = pageSize;
    params.offset = offset;
    params.includeHighFreq = m_lastQueryIncludeHighFreq;

    QFuture<LogQueryResult> future = QtConcurrent::run([params]() {
        return LogQueryTask::execute(params);
    });

    m_historyPageLoadWatcher->setFuture(future);
}

void MainWindow::updateHistoryLogStats()
{
    int totalCount = m_historyLogTableModel->getTotalCount();
    int currentPage = m_historyLogTableModel->getCurrentPage();
    int totalPages = m_historyLogTableModel->getTotalPages();

    ui->totalCountLabel->setText(tr("总记录数: %1").arg(totalCount));
    ui->pageInfoLabel->setText(tr("当前页: %1/%2").arg(currentPage).arg(totalPages));

    ui->firstPageButton->setEnabled(currentPage > 1);
    ui->prevPageButton->setEnabled(currentPage > 1);
    ui->nextPageButton->setEnabled(currentPage < totalPages);
    ui->lastPageButton->setEnabled(currentPage < totalPages);
}

QSet<int> MainWindow::getSelectedHistoryLogLevels() const
{
    QSet<int> levels;
    if (ui->historyDebugCheckBox->isChecked()) levels.insert(static_cast<int>(LogLevel::DEBUG));
    if (ui->historyInfoCheckBox->isChecked()) levels.insert(static_cast<int>(LogLevel::INFO));
    if (ui->historyWarningCheckBox->isChecked()) levels.insert(static_cast<int>(LogLevel::WARNING));
    if (ui->historyErrorCheckBox->isChecked()) levels.insert(static_cast<int>(LogLevel::ERROR));
    if (ui->historyFatalCheckBox->isChecked()) levels.insert(static_cast<int>(LogLevel::FATAL));
    if (ui->historyHighFreqCheckBox->isChecked()) levels.insert(static_cast<int>(LogLevel::HIGHFREQ));
    if (ui->historyCollisionCheckBox->isChecked()) levels.insert(static_cast<int>(LogLevel::COLLISION));
    return levels;
}

LogLevel MainWindow::getMinLogLevel(const QSet<int>& levels) const
{
    if (levels.contains(static_cast<int>(LogLevel::DEBUG))) return LogLevel::DEBUG;
    if (levels.contains(static_cast<int>(LogLevel::INFO))) return LogLevel::INFO;
    if (levels.contains(static_cast<int>(LogLevel::WARNING))) return LogLevel::WARNING;
    if (levels.contains(static_cast<int>(LogLevel::ERROR))) return LogLevel::ERROR;
    if (levels.contains(static_cast<int>(LogLevel::FATAL))) return LogLevel::FATAL;
    if (levels.contains(static_cast<int>(LogLevel::HIGHFREQ))) return LogLevel::HIGHFREQ;
    if (levels.contains(static_cast<int>(LogLevel::COLLISION))) return LogLevel::COLLISION;
    return LogLevel::INFO;
}

void MainWindow::onTodayButtonClicked()
{
    QDateTime now = QDateTime::currentDateTime();
    ui->endDateTimeEdit->setDateTime(now);
    ui->startDateTimeEdit->setDateTime(QDateTime(now.date(), QTime(0, 0, 0)));
}

void MainWindow::onYesterdayButtonClicked()
{
    QDateTime yesterday = QDateTime::currentDateTime().addDays(-1);
    ui->endDateTimeEdit->setDateTime(QDateTime(yesterday.date(), QTime(23, 59, 59)));
    ui->startDateTimeEdit->setDateTime(QDateTime(yesterday.date(), QTime(0, 0, 0)));
}

void MainWindow::onLast7DaysButtonClicked()
{
    QDateTime now = QDateTime::currentDateTime();
    ui->endDateTimeEdit->setDateTime(now);
    ui->startDateTimeEdit->setDateTime(now.addDays(-7));
}

void MainWindow::onLast30DaysButtonClicked()
{
    QDateTime now = QDateTime::currentDateTime();
    ui->endDateTimeEdit->setDateTime(now);
    ui->startDateTimeEdit->setDateTime(now.addDays(-30));
}

void MainWindow::onFirstPage()
{
    loadHistoryPage(1);
}

void MainWindow::onPrevPage()
{
    int currentPage = m_historyLogTableModel->getCurrentPage();
    if (currentPage > 1) {
        loadHistoryPage(currentPage - 1);
    }
}

void MainWindow::onNextPage()
{
    int currentPage = m_historyLogTableModel->getCurrentPage();
    int totalPages = m_historyLogTableModel->getTotalPages();
    if (currentPage < totalPages) {
        loadHistoryPage(currentPage + 1);
    }
}

void MainWindow::onLastPage()
{
    int totalPages = m_historyLogTableModel->getTotalPages();
    if (totalPages > 0) {
        loadHistoryPage(totalPages);
    }
}

void MainWindow::onPageSizeChanged(const QString& pageSizeText)
{
    Q_UNUSED(pageSizeText);
    if (m_hasValidQuery) {
        loadHistoryPage(1);
    }
}

void MainWindow::onExportHistoryLogs()
{
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能导出历史日志。"));
        return;
    }

    if (!m_hasValidQuery) {
        QMessageBox::information(this, tr("提示"), tr("请先执行查询操作"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("导出历史日志"),
        QString("history_logs_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        tr("CSV 文件 (*.csv);;文本文件 (*.txt)"));

    if (fileName.isEmpty()) {
        return;
    }

    bool isCsv = fileName.endsWith(".csv", Qt::CaseInsensitive);

    if (exportHistoryLogs(fileName, isCsv)) {
        QMessageBox::information(this, tr("导出成功"),
            tr("历史日志已成功导出到:\n%1").arg(fileName));
    } else {
        QMessageBox::critical(this, tr("导出失败"),
            tr("导出历史日志失败，请检查文件路径和权限。"));
    }
}

bool MainWindow::exportHistoryLogs(const QString& filePath, bool isCsv)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[MainWindow] 无法打开导出文件:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setCodec(QTextCodec::codecForName("UTF-8"));

    LogQueryParams params;
    params.dbPath = m_logStorage->getDbPath();
    params.startTime = m_lastQueryStartTime;
    params.endTime = m_lastQueryEndTime;
    params.selectedLevels = m_lastQuerySelectedLevels.values().toVector();
    params.source = m_lastQuerySource;
    params.keyword = m_lastQueryKeyword;
    params.includeHighFreq = m_lastQueryIncludeHighFreq;

    if (isCsv) {
        out << "\xEF\xBB\xBF";
        out << "时间,级别,来源,消息\n";

        int totalCount = m_historyLogTableModel->getTotalCount();
        int pageSize = 500;
        for (int offset = 0; offset < totalCount; offset += pageSize) {
            params.limit = pageSize;
            params.offset = offset;
            LogQueryResult result = LogQueryTask::execute(params);

            for (const auto& entry : result.results) {
                QString msg = entry.message;
                msg.replace("\"", "\"\"");
                out << "\"" << entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << "\","
                    << "\"" << LogUtils::levelToString(entry.level) << "\","
                    << "\"" << (entry.source.isEmpty() ? "系统" : entry.source) << "\","
                    << "\"" << msg << "\"\n";
            }
        }
    } else {
        out << "历史日志导出\n";
        out << "时间范围: " << m_lastQueryStartTime.toString("yyyy-MM-dd hh:mm:ss")
            << " 至 " << m_lastQueryEndTime.toString("yyyy-MM-dd hh:mm:ss") << "\n";
        QStringList levelNames;
        for (int level : m_lastQuerySelectedLevels) {
            levelNames << LogUtils::levelToString(static_cast<LogLevel>(level));
        }
        out << "选中级别: " << levelNames.join(", ") << "\n";
        if (!m_lastQuerySource.isEmpty()) {
            out << "来源: " << m_lastQuerySource << "\n";
        }
        if (!m_lastQueryKeyword.isEmpty()) {
            out << "关键词: " << m_lastQueryKeyword << "\n";
        }
        out << "============================================================================\n\n";

        int totalCount = m_historyLogTableModel->getTotalCount();
        int pageSize = 500;
        for (int offset = 0; offset < totalCount; offset += pageSize) {
            params.limit = pageSize;
            params.offset = offset;
            LogQueryResult result = LogQueryTask::execute(params);

            for (const auto& entry : result.results) {
                out << "[" << entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz") << "] "
                    << "[" << LogUtils::levelToString(entry.level) << "] "
                    << "[" << (entry.source.isEmpty() ? "系统" : entry.source) << "] "
                    << entry.message << "\n";
            }
        }
    }

    file.close();
    return true;
}

void MainWindow::onClearHistoryLogs()
{
    if (!m_userAuthManager || !m_userAuthManager->canAdmin()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要管理员权限才能清空历史日志。"));
        return;
    }

    auto reply = QMessageBox::question(this, tr("确认清空"),
        tr("确定要清空所有历史日志吗？\n\n此操作不可恢复！"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (m_logStorage->clearLogs()) {
            m_historyLogTableModel->clear();
            m_hasValidQuery = false;
            updateHistoryLogStats();
            QMessageBox::information(this, tr("清空成功"), tr("所有历史日志已清空"));
            qDebug() << "[MainWindow] 历史日志已清空";
        } else {
            QMessageBox::critical(this, tr("清空失败"), tr("清空历史日志失败"));
            qWarning() << "[MainWindow] 清空历史日志失败";
        }
    }
}

void MainWindow::onSelectAllLevels()
{
    ui->historyDebugCheckBox->setChecked(true);
    ui->historyInfoCheckBox->setChecked(true);
    ui->historyWarningCheckBox->setChecked(true);
    ui->historyErrorCheckBox->setChecked(true);
    ui->historyFatalCheckBox->setChecked(true);
    ui->historyHighFreqCheckBox->setChecked(true);
    ui->historyCollisionCheckBox->setChecked(true);
}

void MainWindow::onDeselectAllLevels()
{
    ui->historyDebugCheckBox->setChecked(false);
    ui->historyInfoCheckBox->setChecked(false);
    ui->historyWarningCheckBox->setChecked(false);
    ui->historyErrorCheckBox->setChecked(false);
    ui->historyFatalCheckBox->setChecked(false);
    ui->historyHighFreqCheckBox->setChecked(false);
    ui->historyCollisionCheckBox->setChecked(false);
}

void MainWindow::setParamSpinBoxColor(const QString& key, Nav2ParameterThread::ParamStatus status)
{
    QDoubleSpinBox* spinBox = findChild<QDoubleSpinBox*>(key);
    if (!spinBox) return;

    QString styleSheet;
    switch (status) {
        case Nav2ParameterThread::ParamStatus::Pending:
            styleSheet = "QDoubleSpinBox { background-color: #e3f2fd; }";  // 浅蓝色
            break;
        case Nav2ParameterThread::ParamStatus::Success:
            styleSheet = "QDoubleSpinBox { background-color: #e8f5e9; }";  // 浅绿色
            break;
        case Nav2ParameterThread::ParamStatus::Failed:
            styleSheet = "QDoubleSpinBox { background-color: #ffebee; }";  // 浅红色
            break;
        default:
            styleSheet = "";  // 默认颜色
            break;
    }
    spinBox->setStyleSheet(styleSheet);
}

void MainWindow::onManualInterventionReceived(bool needsIntervention)
{
    if (needsIntervention) {
        updateInterventionStatus(Intervention);
        showInterventionDialog();
    } else if (m_interventionStatus == Intervention) {
        updateInterventionStatus(Normal);
    }
}

void MainWindow::updateInterventionStatus(InterventionStatus status)
{
    m_interventionStatus = status;
    QString color;
    switch (status) {
        case Normal:
            color = "rgb(0, 255, 0)";  // 绿色
            break;
        case Recovering:
            color = "rgb(255, 255, 0)";  // 黄色
            break;
        case Intervention:
            color = "rgb(255, 0, 0)";  // 红色
            break;
    }
    ui->interventionIndicator->setStyleSheet(
        QString("background-color: %1; border-radius: 8px; border: 1px solid #333;").arg(color));
}

void MainWindow::showInterventionDialog()
{
    if (m_interventionDialog && m_interventionDialog->isVisible()) {
        return;
    }

    delete m_interventionDialog;
    m_interventionDialog = new QDialog(this);
    m_interventionDialog->setWindowTitle("人工干预请求");
    m_interventionDialog->setModal(false);
    m_interventionDialog->setStyleSheet("QDialog { background-color: #ffeeee; }");

    QVBoxLayout* layout = new QVBoxLayout(m_interventionDialog);

    QLabel* iconLabel = new QLabel("⚠", m_interventionDialog);
    iconLabel->setStyleSheet("font-size: 48px; color: red;");
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel* msgLabel = new QLabel("机器人需要人工干预！\n请检查机器人状态并处理异常情况。", m_interventionDialog);
    msgLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #cc0000;");
    msgLabel->setAlignment(Qt::AlignCenter);

    QPushButton* ackBtn = new QPushButton("已知晓", m_interventionDialog);
    ackBtn->setStyleSheet("background-color: #cc0000; color: white; font-weight: bold; padding: 10px;");

    layout->addWidget(iconLabel);
    layout->addWidget(msgLabel);
    layout->addWidget(ackBtn);

    connect(ackBtn, &QPushButton::clicked, m_interventionDialog, &QDialog::accept);
    m_interventionDialog->setLayout(layout);
    m_interventionDialog->show();
    m_interventionDialog->raise();
    m_interventionDialog->activateWindow();
}

void MainWindow::onRunWaypoints()
{
    // 权限检查
    if (!m_userAuthManager || !m_userAuthManager->canOperate()) {
        QMessageBox::warning(this, tr("权限不足"),
            tr("您需要操作员或管理员权限才能执行此操作。"));
        return;
    }

    // 检查是否已在运行
    if (m_waypointProcess && m_waypointProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(this, tr("提示"), tr("预设路径任务正在运行中..."));
        return;
    }

    // 创建进程并启动
    if (!m_waypointProcess) {
        m_waypointProcess = new QProcess(this);
        connect(m_waypointProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                    Q_UNUSED(exitStatus);
                    QString message = (exitCode == 0) ? tr("预设路径任务完成") : tr("预设路径任务失败 (退出码: %1)").arg(exitCode);
                    statusBar()->showMessage(message, 5000);
                    m_waypointProcess->deleteLater();
                    m_waypointProcess = nullptr;
                });
    }

    // source ROS 2 环境并运行（使用默认路径文件）
    QString setupPath = QString::fromStdString(ament_index_cpp::get_package_prefix("robot_navigation2")) + "/setup.bash";
    QString command = "source " + setupPath + " && "
                       "ros2 run robot_navigation2 waypoint_runner_client";

    m_waypointProcess->start("bash", QStringList() << "-c" << command);

    if (m_waypointProcess->waitForStarted()) {
        statusBar()->showMessage(tr("预设路径任务已启动..."), 3000);
    } else {
        QMessageBox::warning(this, tr("启动失败"), tr("无法启动预设路径任务"));
    }
}
