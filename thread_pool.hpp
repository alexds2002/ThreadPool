#pragma once

#include "thread_pool.h"
#include <memory>

/**
 * @brief Constructs a ThreadPool with a specified number of threads.
 *
 * This constructor initializes a thread pool with a given number of threads.
 * If the specified number of threads is invalid (e.g., less than 1), the pool
 * will default to one thread. Each thread will continuously execute tasks
 * from the task queue until the pool is stopped.
 *
 * @param _number_of_threads The desired number of threads for the thread pool.
 *                           If the value is less than or equal to 1, the pool
 *                           will use a single thread.
 *
 * @note
 * - The thread pool is initialized with a task loop in each thread that waits
 *   for tasks to be added. Each thread will continue to run until a forced stop
 *   is triggered (`m_force_stop` is true) and all tasks in the queue are processed.
 * - The constructor ensures that the number of threads is set to at least 1
 *   (in case the user passes an invalid value like 0).
 *
 * @details
 * - The constructor creates worker threads and assigns them a task loop. Each
 *   thread waits for tasks to be added to the task queue using a condition
 *   variable (`m_condition_var`).
 * - When a task is available or a stop is requested, the thread wakes up. If a
 *   forced stop is triggered and the task queue is empty, the thread exits its loop.
 * - The task queue is thread-safe and is protected by a mutex (`m_mutex`) to
 *   ensure safe access by multiple threads.
 *
 * Example usage:
 * @code
 * ThreadPool pool(4);  // Creates a thread pool with 4 threads
 * @endcode
 */
inline ThreadPool::ThreadPool(uint32_t _number_of_threads) noexcept : m_threads_count(_number_of_threads),
                                                               m_force_stop(false)
{
    /* hardware_concurrency returns 0 when it fails or if an invalid value is passed */
    if(_number_of_threads <= 1)
    {
        m_threads_count = 1;
    }
    for(size_t i = 0; i < m_threads_count; ++i)
    {
        m_workers.emplace_back([this](){
            while(true)
            {
                std::function<void()> task;
                {
                    /* lock critical code */
                    std::unique_lock<std::mutex> lock(this->m_mutex);
                    /* condition variable unlocks while waiting */
                    this->m_condition_var.wait(lock,
                            [this]()
                            {
                                return this->m_force_stop || !this->m_tasks.empty();
                            });
                    /* condition variable locks after being awaken from notify_one() */
                    if(this->m_force_stop && this->m_tasks.empty())
                    {
                        return;
                    }
                    task = std::move(this->m_tasks.front());
                    this->m_tasks.pop();
                }
                task();
            }});
        }
}

inline ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex>lock(m_mutex);
        m_force_stop = true;
    }
    m_condition_var.notify_all();
    for(auto& worker : m_workers)
    {
        worker.join();
    }
}

/**
 * @brief Adds a task to the thread pool and returns a future for the result.
 *
 * This function allows you to enqueue a task to the thread pool. The task is
 * represented by a callable (a function, lambda, or functor) and its associated
 * arguments. The task will be executed asynchronously by one of the threads in
 * the pool, and a `std::future` is returned, which allows you to retrieve the
 * result of the task once it has been completed.
 *
 * @tparam Callable The type of the callable (e.g., a function, lambda, or functor).
 * @tparam Args The types of the arguments passed to the callable.
 * @param func The callable object (function, lambda, etc.) that represents the task to be executed.
 * @param args The arguments to be passed to the callable when it is executed.
 * @return A `std::future` that holds the result of the task. The type of the future is deduced based on the return type of the callable.
 *
 * @throws std::runtime_error if a task is added after the thread pool has been forcefully stopped.
 *
 * @details
 * - The function first deduces the return type of the callable using `std::result_of` and creates a packaged task from it.
 * - The packaged task is then added to the internal task queue (`m_tasks`), which is shared among all threads.
 * - A condition variable (`m_condition_var`) is notified to wake up one of the threads to process the task.
 * - The function ensures that tasks are only added if the thread pool has not been forcefully stopped.
 *
 * Example usage:
 * @code
 * auto result = threadPool.Add_Task([](int a, int b){ return a + b; }, 5, 10);
 * std::cout << "Result: " << result.get() << std::endl; // Output: Result: 15
 * @endcode
 *
 * @note
 * - The `std::future` returned by this function allows you to retrieve the result of the task in the future, once the task completes.
 * - You should be cautious when calling this function after the thread pool is stopped, as it will throw a runtime exception.
 */
template<typename Callable, typename... Args>
auto ThreadPool::Add_Task(Callable&& func, Args... args) ->
                std::future<typename std::result_of<Callable(Args...)>::type>
{
    using return_type = typename std::result_of<Callable(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<Callable>(func), std::forward<Args>(args)...));
    std::future<return_type> result = task->get_future();
    {
        /* lock shrared data */
        std::lock_guard<std::mutex> lock(m_mutex);
        /* dont allow task adding after force stop */
        if(m_force_stop)
        {
            throw std::runtime_error("Trying to add taks after forced stop!");
        }
        m_tasks.emplace([task](){ (*task)(); });
    }
    /* wake up a thread if its waiting(should not be locked, can create a "hurry up and wait" scenario) */
    m_condition_var.notify_one();
    return result;
}

inline uint32_t ThreadPool::Get_Number_Of_Threads() const noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_threads_count;
}

inline uint32_t ThreadPool::Busy_Threads() const noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_busy_threads;
}

inline std::size_t ThreadPool::Number_Of_Tasks() const noexcept
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_tasks.size();
}
