#include "roscontextmanager.h"
#include <QDebug>

ROSContextManager& ROSContextManager::instance()
{
    static ROSContextManager instance;
    return instance;
}

ROSContextManager::ROSContextManager()
    : m_initialized(false)
{
    qDebug() << "[ROSContextManager] 构造函数";
}

ROSContextManager::~ROSContextManager()
{
    shutdown();
}

void ROSContextManager::initialize()
{
    std::call_once(m_initFlag, [this]() {
        try {
            rclcpp::init(0, nullptr);
            m_initialized.store(true);
            qDebug() << "[ROSContextManager] 初始化成功";
        } catch (const std::exception& e) {
            qCritical() << "[ROSContextManager] 初始化异常:" << e.what();
            m_initialized.store(false);
            throw;
        } catch (...) {
            qCritical() << "[ROSContextManager] 初始化未知异常";
            m_initialized.store(false);
            throw;
        }
    });
}

bool ROSContextManager::isInitialized() const
{
    return m_initialized.load();
}

void ROSContextManager::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized.load()) {
        if (rclcpp::ok()) {
            try {
                rclcpp::shutdown();
            } catch (const std::exception& e) {
                qWarning() << "[ROSContextManager] shutdown异常:" << e.what();
            } catch (...) {
                qWarning() << "[ROSContextManager] shutdown未知异常";
            }
        }
        m_initialized.store(false);
    }
}
