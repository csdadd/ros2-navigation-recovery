#ifndef ROSCONTEXTMANAGER_H
#define ROSCONTEXTMANAGER_H

#include <rclcpp/rclcpp.hpp>

#include <atomic>
#include <mutex>

class ROSContextManager
{
public:
    static ROSContextManager& instance();

    void initialize();
    bool isInitialized() const;
    void shutdown();

    ROSContextManager(const ROSContextManager&) = delete;
    ROSContextManager& operator=(const ROSContextManager&) = delete;

private:
    ROSContextManager();
    ~ROSContextManager();

    std::once_flag m_initFlag;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_initialized;
};

#endif // ROSCONTEXTMANAGER_H
