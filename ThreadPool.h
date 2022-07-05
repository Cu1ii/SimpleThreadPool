//
// Created by Cu1 on 2022/7/4.
//

#ifndef SIMPLETHREADPOOL_THREADPOOL_H
#define SIMPLETHREADPOOL_THREADPOOL_H


#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool
{
public:
    ThreadPool(size_t threads);

    template<class F, class... Args>
    auto addTask(F&& f, Args&& ...args) -> std::future<typename std::result_of<F(Args...)>::type>;

    ~ThreadPool();
private:
    std::vector<std::thread> thread_workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable conditionVariable;
    bool if_stop;
};


inline ThreadPool::ThreadPool(size_t threads) : if_stop(false)
{
    for (size_t i = 0; i < threads; i++)
        thread_workers.emplace_back(
                [this] {
                    while (1)
                    {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            conditionVariable.wait(lock,
                                                   [this] { return this->if_stop || !this->tasks.empty(); });
                            if (this->if_stop && this->tasks.empty())
                                return ;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }

                        task();
                    }
                });
}


template <class F, class ...Args>
auto ThreadPool::addTask(F&& f, Args&& ...args) -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = new std::packaged_task<return_type()>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (if_stop) {
            throw std::runtime_error("the ThreadPool stopped");
        }
        tasks.emplace(
                [task](){
                    try {
                        (*task)();
                    }
                    catch (...)
                    {
                        delete task;
                    }
                });
    }
    conditionVariable.notify_one();
    return res;
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if_stop = true;
    }
    conditionVariable.notify_all();
    for (auto& thread: thread_workers)
        thread.join();
}

#endif //SIMPLETHREADPOOL_THREADPOOL_H
