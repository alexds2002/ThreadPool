#pragma once

/*
 * thread_pool is class for running any tasks you want concurrently by starting number of threads
 * based on std::thread::hardware_concurrency. The thread_pool class supports result retrivial via
 * std::future, it also supports variadic number of arguments to be passed to the task.
 * By default the thread_pool starts with 2 threads if less are passed.
 *
 * BENCHMARK:
 * This implementation is between 15-25% slower in adding a new task then the simple version
 * without support for passing args or result retrivial.
 * The execution of the tasks is the same.
 *
 * On avarage 4700ms to create 1000 threads with 1000 tasks each
 * 1'000'000 tasks in total
 *
 * The simple implementation also doesnt need to be templated,
 * which obviously decreases compilation time.
 *
 * !!! WARNINGS !!!
 * The thread pool is thread safe by itself but the data
 * and functionality inside the tasks is NOT thread safe.
 * It is up to the user to avoid race conditions, deadlocks
 * and maintain synchronization in their implementations.
 * CONTRIBUTORS
 * Stefan Rachkov
 */

#include <condition_variable>
#include <type_traits>
#include <functional>
#include <cstdint>
#include <future>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>

class ThreadPool
{
public:
    /* creates and starts the threads */
    ThreadPool(uint32_t _number_of_threads) noexcept;

    /* joins all the threads, force stops and notifyes all the threads */
    ~ThreadPool();

    /**
     * @brief Adds a task to the task queue
     *
     * @param  Callable&&: taks to be executed
     * @param  Args&&: arguments of the Callable
     *
     * Example usage:
     *
     * @return void
     */
    template<typename Callable, typename... Args>
    auto Add_Task(Callable&& func, Args... args) -> std::future<typename std::result_of<Callable(Args...)>::type>;

    /*
     * @brief Get the max number of worker threads for this system
     * possably based on std::thread::hardware_concurrency
     *
     * @return void
     */
    uint32_t Get_Number_Of_Threads() const noexcept;

    /*
     * @brief Get number of threads currently working
     *
     * @return uint32_t: count
     */
    uint32_t Busy_Threads() const noexcept;

    /*
     * @brief Get number of tasks on the queue
     *
     * @return uint32_t: count
     */
    std::size_t Number_Of_Tasks() const noexcept;

private:
    /* lock shared data */
    mutable std::mutex m_mutex;
    /* used to notify a thread in case it is waiting(this may happen outside of your program) */
    std::condition_variable m_condition_var;
    /* m_workers size should be based on the system it is running(maybe std::thread::hardware_concurrency) */
    std::vector<std::thread> m_workers;
    /* a queue of tasks where AddTask pushes new tasks and the executor removes tasks as soon as there is a free thread */
    std::queue<std::function<void()>> m_tasks;

    /* number of threads that are currenty working */
    uint32_t m_busy_threads{0};
    /* number of threads based on std::thread::hardware_concurrency */
    uint32_t m_threads_count{0};
    /* if there is a force quit you can still finish your current execution */
    bool m_force_stop{false};
};

/* contains the function definitions */
#include "thread_pool.hpp"

