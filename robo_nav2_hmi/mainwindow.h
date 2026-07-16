#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThreadPool>
#include <QStandardPaths>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QProcess>
#include <memory>
#include "robotstatusthread.h"
#include "systemmonitorthread.h"
#include "logthread.h"
#include "logtablemodel.h"
#include "logfilterproxymodel.h"
#include "logquerytask.h"
#include "historylogmodel.h"
#include "mapcache.h"
#include "nav2viewwidget.h"
#include "nav2viewdataprocessor.h"
#include "navigationactionthread.h"
#include "pathvisualizer.h"
#include "userstorageengine.h"
#include "userauthmanager.h"
#include "logindialog.h"
#include "usermanagementdialog.h"
#include "changepassworddialog.h"
#include "user.h"
#include "nav2parameterthread.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    /**
     * @brief 初始化主窗口
     * @return 初始化成功返回true，失败返回false
     * @details 两阶段初始化，避免构造函数中调用exit或close
     */
    bool initialize();

private slots:
    // RobotStatusThread槽函数
    void onBatteryStatusReceived(float voltage, float percentage);
    void onPositionReceived(double x, double y, double yaw);
    void onOdometryReceived(double x, double y, double yaw, double vx, double vy, double omega);
    void onSystemTimeReceived(const QString& time);
    void updateCurrentTime();

    // SystemMonitorThread槽函数
    void onLogMessageReceived(const QString& message, int level, const QDateTime& timestamp);
    void onCollisionDetected(const QString& message);
    void onExceptionDetected(const QString& message);
    void onBehaviorTreeLogReceived(const QString& log);

    // LogThread槽函数
    void onLogFileChanged(const QString& filePath);

    // 线程状态槽函数
    void onConnectionStateChanged(bool connected);
    void onThreadStarted(const QString& threadName);
    void onThreadStopped(const QString& threadName);
    void onThreadError(const QString& error);

    // 日志过滤槽函数
    void onFilterChanged();
    void onClearLogByLevel();
    void onPauseLogToggled(bool checked);

    // 历史日志槽函数
    void onHistoryLogQuery();
    void onHistoryLogQueryFinished();
    void onHistoryLogQueryFailed(const QString& error);
    void onHistoryPageLoadFinished();
    void onTodayButtonClicked();
    void onYesterdayButtonClicked();
    void onLast7DaysButtonClicked();
    void onLast30DaysButtonClicked();
    void onFirstPage();
    void onPrevPage();
    void onNextPage();
    void onLastPage();
    void onPageSizeChanged(const QString& pageSizeText);
    void onExportHistoryLogs();
    void onClearHistoryLogs();
    void onSelectAllLevels();
    void onDeselectAllLevels();

    // 导航相关槽函数
    void onStartNavigation();
    void onCancelNavigation();
    void onClearGoal();
    void onRunWaypoints();
    void onNavigationFeedback(double distanceRemaining, double navigationTime, int recoveries, double estimatedTimeRemaining);
    void onNavigationResult(bool success, const QString& message);
    void onGoalAccepted();
    void onGoalRejected(const QString& reason);
    void onGoalCanceled();

    // 人工干预状态槽函数
    void onManualInterventionReceived(bool needsIntervention);

    // 用户权限管理槽函数
    void onLoginSuccess(const User& user);
    void onLoginFailed(const QString& reason);
    void onLogout();
    void onUserManagement();
    void onChangePassword();
    void updateUIBasedOnPermission();

    // Nav2ParameterThread 槽函数
    void onRefreshButtonClicked();
    void onApplyButtonClicked();
    void onResetButtonClicked();
    void onDiscardButtonClicked();
    void onParameterRefreshed(bool success, const QString& message);
    void onParameterApplied(bool success, const QString& message, const QStringList& appliedKeys, const QStringList& failedKeys);
    void onParameterOperationFinished(const QString& operation, bool success, const QString& message);
    void onParameterValueChanged(double value);

private:
    // 人工干预状态枚举
    enum InterventionStatus { Normal, Recovering, Intervention };

private:
    // 初始化函数
    bool initializeUserSystem();
    void initializeLogSystem();
    void initializeROSContext();
    void initializeUI();
    bool handleLogin();

    // 线程管理与辅助函数
    void initializeThreads();
    void connectSignals();
    void startAllThreads();
    void stopAllThreads();
    Q_INVOKABLE void refreshLogDisplay(bool autoScroll = true);
    void updateParameterValue(const QString& key, const QVariant& value);
    void addLogEntry(const LogEntry& entry);
    void setParamSpinBoxColor(const QString& key, Nav2ParameterThread::ParamStatus status);

    // 地图辅助函数
    static QString getMapPathFromRosParam(rclcpp::Node::SharedPtr node);

    // 人工干预状态相关
    void updateInterventionStatus(InterventionStatus status);
    void showInterventionDialog();

    // 历史日志辅助函数
    void loadHistoryPage(int page);
    void updateHistoryLogStats();
    QSet<int> getSelectedHistoryLogLevels() const;
    LogLevel getMinLogLevel(const QSet<int>& levels) const;
    bool exportHistoryLogs(const QString& filePath, bool isCsv);

private:
    Ui::MainWindow *ui;
    
    static const int THREAD_START_DELAY_MS = 100;     // 线程启动延迟时间
    static const int THREAD_STOP_TIMEOUT_MS = 3000;   // 线程停止超时时间
    
    // 线程和日志相关
    // 使用智能指针管理线程生命周期，避免内存泄漏和悬空指针
    std::unique_ptr<RobotStatusThread> m_robotStatusThread;
    std::unique_ptr<SystemMonitorThread> m_systemMonitorThread;
    std::unique_ptr<LogThread> m_logThread;
    std::unique_ptr<LogStorageEngine> m_logStorage;
    std::unique_ptr<LogTableModel> m_logTableModel;     // UI 显示层：QTableView 的数据源 (最大 1000 条)
    std::unique_ptr<LogFilterProxyModel> m_logFilterProxyModel;  // 日志过滤代理模型

    // 历史日志相关
    std::unique_ptr<HistoryLogTableModel> m_historyLogTableModel;
    std::unique_ptr<LogFilterProxyModel> m_historyLogFilterProxyModel;
    QFutureWatcher<LogQueryResult>* m_historyLogQueryWatcher;
    QFutureWatcher<LogQueryResult>* m_historyPageLoadWatcher;

    // 查询条件缓存
    QDateTime m_lastQueryStartTime;
    QDateTime m_lastQueryEndTime;
    QSet<int> m_lastQuerySelectedLevels;
    QString m_lastQuerySource;
    QString m_lastQueryKeyword;
    bool m_lastQueryIncludeHighFreq;
    bool m_hasValidQuery = false;

    // 内存中的完整日志缓存，用于日志管理和按级别清除等操作 (最大 10000 条)
    // 设计目的：分离数据缓存层(m_allLogs)和显示层(m_logTableModel)，使得内存中保留更多历史日志，
    //          而UI显示保持轻量，避免界面卡顿
    // 里程计日志(ODOMETRY)不显示在 UI 中，也不进入此缓存，直接存储到独立数据库表
    QList<LogEntry> m_allLogs;
    static const int MAX_ALL_LOGS_SIZE = 10000;
    bool m_logPaused = false;  // 日志接收暂停标志

    // 导航视图和地图相关
    std::string m_mapPath;
    std::unique_ptr<Nav2ViewWidget> m_nav2ViewWidget;
    std::unique_ptr<Nav2ViewDataProcessor> m_nav2ViewProcessor;
    std::unique_ptr<MapCache> m_mapCache;
    std::unique_ptr<NavigationActionThread> m_navigationActionThread;
    std::unique_ptr<PathVisualizer> m_pathVisualizer;

    // 目标点相关
    double m_targetX;
    double m_targetY;
    double m_targetYaw;
    bool m_hasTarget;
    QDateTime m_startTime;
    double m_initialDistance;

    // 用户认证模块
    std::unique_ptr<UserStorageEngine> m_userStorageEngine;
    std::unique_ptr<UserAuthManager> m_userAuthManager;
    std::unique_ptr<LoginDialog> m_loginDialog;
    std::unique_ptr<UserManagementDialog> m_userManagementDialog;
    std::unique_ptr<Nav2ParameterThread> m_paramThread;

    // 人工干预状态相关
    InterventionStatus m_interventionStatus = Normal;
    int m_lastRecoveries = 0;
    QDialog* m_interventionDialog = nullptr;

    // 预设路径运行相关
    QProcess* m_waypointProcess = nullptr;
};

#endif // MAINWINDOW_H
