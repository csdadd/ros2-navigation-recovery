#ifndef THREADSAFEQUEUE_H
#define THREADSAFEQUEUE_H

#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QMutexLocker>

template<typename T>
class ThreadSafeQueue
{
public:
    enum class OverflowPolicy {
        Block,    // 阻塞等待
        Discard   // 丢弃并返回false
    };

    explicit ThreadSafeQueue(int capacity = 0, OverflowPolicy policy = OverflowPolicy::Block)
        : m_capacity(capacity), m_overflowPolicy(policy) {}

    bool enqueue(const T& value)
    {
        QMutexLocker locker(&m_mutex);

        if (m_capacity > 0 && m_queue.size() >= m_capacity) {
            if (m_overflowPolicy == OverflowPolicy::Discard) {
                return false;
            }
            while (m_queue.size() >= m_capacity) {
                m_notFull.wait(&m_mutex);
            }
        }

        m_queue.enqueue(value);
        m_condition.wakeOne();
        return true;
    }

    bool enqueue(T&& value)
    {
        QMutexLocker locker(&m_mutex);

        if (m_capacity > 0 && m_queue.size() >= m_capacity) {
            if (m_overflowPolicy == OverflowPolicy::Discard) {
                return false;
            }
            while (m_queue.size() >= m_capacity) {
                m_notFull.wait(&m_mutex);
            }
        }

        m_queue.enqueue(std::move(value));
        m_condition.wakeOne();
        return true;
    }

    T dequeue()
    {
        QMutexLocker locker(&m_mutex);
        while (m_queue.isEmpty()) {
            m_condition.wait(&m_mutex);
        }

        if (m_capacity > 0 && m_queue.size() == m_capacity) {
            m_notFull.wakeOne();
        }

        return m_queue.dequeue();
    }

    bool tryDequeue(T& value, int timeout = 100)
    {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty()) {
            m_condition.wait(&m_mutex, timeout);
        }
        if (!m_queue.isEmpty()) {
            value = m_queue.dequeue();

            if (m_capacity > 0 && m_queue.size() == m_capacity - 1) {
                m_notFull.wakeOne();
            }

            return true;
        }
        return false;
    }

    bool isEmpty() const
    {
        QMutexLocker locker(&m_mutex);
        return m_queue.isEmpty();
    }

    bool isFull() const
    {
        QMutexLocker locker(&m_mutex);
        return m_capacity > 0 && m_queue.size() >= m_capacity;
    }

    int size() const
    {
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }

    int capacity() const
    {
        return m_capacity;
    }

    void setCapacity(int capacity)
    {
        QMutexLocker locker(&m_mutex);
        m_capacity = capacity;
    }

    void clear()
    {
        QMutexLocker locker(&m_mutex);
        m_queue.clear();
        m_notFull.wakeAll();
    }

private:
    QQueue<T> m_queue;
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    QWaitCondition m_notFull;
    int m_capacity = 0;
    OverflowPolicy m_overflowPolicy = OverflowPolicy::Block;
};

#endif // THREADSAFEQUEUE_H
