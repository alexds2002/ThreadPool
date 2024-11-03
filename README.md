# ThreadPool

The library contains a Thread Pool with a N number of threads(hardware_concurrency recommended).

Usage:
#include "thread_pool.h"

void main()
{
    ThreadPool pool(std::thread::hardware_concurrency()); // Create thread with the CPU`s cores number of threads

    pool.Add_Task([](){ std::cout << "This is executed in the thread pool\n"; };

    return 0;
}
