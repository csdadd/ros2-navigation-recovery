#include "basethread.h"
#include <rclcpp/rclcpp.hpp>
#include <QDebug>

BaseThread::BaseThread(QObject* parent)
    : QThread(parent)
    , m_running(false)
    , m_stopped(false)
    , m_threadName("BaseThread")
{
    qDebug() << "[BaseThread] 构造函数 - 线程名称:" << m_threadName;
}

BaseThread::~BaseThread()
{
    stopThread();

    // 等待spin线程结束
    if (m_spinThread && m_spinThread->joinable()) {
        m_spinThread->join();
    }

    // 使用超时等待，避免永久阻塞
    if (!wait(WAIT_TIMEOUT_MS)) {
        qWarning() << "[BaseThread] 析构函数 - 等待线程结束超时，强制终止:" << m_threadName;
        terminate();
        wait();
    }
}

void BaseThread::stopThread()
{
    // 先设置m_running为false，再设置m_stopped为true，避免竞态条件
    m_running = false;
    m_stopped = true;
}

bool BaseThread::isThreadRunning() const
{
    return m_running;
}

void BaseThread::run()
{
    m_running = true;
    m_stopped = false;

    qDebug() << "[BaseThread] 线程开始运行 -" << m_threadName;
    emit threadStarted(m_threadName);//记录和显示日志

    try {
        initialize();

        // 如果子类使用阻塞 spin() 模式，不需要创建额外的 spin 线程
        if (!usesBlockingSpin()) {
            // 使用unique_ptr管理线程生命周期
            m_spinThread = std::make_unique<std::thread>([this]() {
                while (rclcpp::ok() && !m_stopped) {
                    if (m_executor) {
                        // 使用spin_once替代spin，避免阻塞
                        m_executor->spin_once(std::chrono::milliseconds(SPIN_TIMEOUT_MS));
                    } else {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
            });
        }

        while (!m_stopped && !isInterruptionRequested()) {
            process();
            msleep(10);
        }

        // 先停止spin线程
        m_stopped = true;

        // 等待spin线程结束（仅当创建了spin线程时）
        if (!usesBlockingSpin()) {
            if (m_spinThread && m_spinThread->joinable()) {
                m_spinThread->join();
            }
        }

        cleanup();

    } catch (const std::exception& e) {
        qCritical() << "[BaseThread] 异常捕获 -" << m_threadName << "-" << e.what();
        emit threadError(QString("Exception in %1: %2").arg(m_threadName).arg(e.what()));
    } catch (...) {
        qCritical() << "[BaseThread] 未知异常 -" << m_threadName;
        emit threadError(QString("Unknown exception in %1").arg(m_threadName));
    }

    m_running = false;
    qDebug() << "[BaseThread] 线程已停止 -" << m_threadName;
    emit threadStopped(m_threadName);
}
