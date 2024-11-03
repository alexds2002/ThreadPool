#pragma once

/*
 * A simple not very flexable yet performent implementation of a thread pool.
 * TODO(Alex): To be further tested...
 */

#include <condition_variable>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <cinttypes>

namespace CherryTest
{

class ThreadPoolSimple
{
public:
    ThreadPoolSimple(uint32_t _threads_count);
    ~ThreadPoolSimple();
    void Add_Task(std::function<void()>&& func);
private:
    mutable std::mutex m_mutex;
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    uint32_t m_threads_count;
    uint32_t m_busy_threads;
    std::condition_variable m_condition_var;
    bool m_force_stop;
};

inline ThreadPoolSimple::ThreadPoolSimple(uint32_t _threads_count) :
                                        m_threads_count(_threads_count),
                                        m_force_stop(false)
{
    if(m_threads_count == 0)
    {
        m_threads_count = 1;
    }
    for(int i = 0; i < m_threads_count; ++i)
    {
        m_workers.emplace_back([this]
        {
            while(true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_condition_var.wait(lock, [this]()
                            {
                                return m_force_stop || !m_tasks.empty();
                            });
                    if(this->m_force_stop || this->m_tasks.empty())
                    {
                        return;
                    }
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                task();
            }
        });
    }
}

inline ThreadPoolSimple::~ThreadPoolSimple()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_force_stop = true;
    }
    m_condition_var.notify_all();
    for(auto& worker : m_workers)
    {
        worker.join();
    }
}

inline void ThreadPoolSimple::Add_Task(std::function<void()>&& func)
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_tasks.emplace(func);
    }
    m_condition_var.notify_one();
}

} /* namespace CherryTest */
