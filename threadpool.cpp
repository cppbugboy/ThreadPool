#include <unistd.h>
#include <iostream>

#include "threadpool.h"

ThreadPool::ThreadPool()
{
}

ThreadPool::~ThreadPool()
{
    std::map<std::thread::id, std::thread *>::iterator it = thread_map_.begin();
    for (it = thread_map_.begin(); it != thread_map_.end(); it++)
    {
        std::thread *thread = it->second;
        if (thread->joinable())
        {
            thread->join();
        }
    }
}

ThreadPool *ThreadPool::Instance()
{
    static ThreadPool *threadpool = new ThreadPool();
    return threadpool;
}

void ThreadPool::ThreadPoolInit(const unsigned int max_threads, const unsigned int init_threads, const unsigned int max_tasks)
{
    max_threads_ = max_threads;
    max_tasks_ = max_tasks;
    init_threads_ = init_threads;

    current_threads_.store(0);
    current_tasks_.store(0);
    isquit_.store(false);
    stop_.store(false);

    for (int i = 0; i < init_threads; i++)
    {
        std::thread *work_thread = new std::thread(&ThreadPool::WorkThread, this);
        {
            std::lock_guard<std::mutex> lock_guard(threads_mutex_);
            thread_map_.insert(std::pair<std::thread::id, std::thread *>(work_thread->get_id(), work_thread));
        }
        current_threads_.fetch_add(1);
        idle_threads_.push(work_thread->get_id());
    }
    // 指定第一个线程为领导者线程，并从追随者线程容器中移除
    leader_id_ = idle_threads_.front();
    idle_threads_.pop();
}

void ThreadPool::WorkThread()
{
    // 获取任务并执行
    auto task_fun = [this](std::unique_lock<std::mutex> &unique_lock)
    {
        std::function<void()> task = std::move(tasks_queue_.front());
        tasks_queue_.pop();
        unique_lock.unlock();
        current_tasks_.fetch_sub(1);
        task();
    };

    while (true)
    {
        if (stop_.load())
        {
            break;
        }

        //如果当前线程是领导者线程
        if (leader_id_ == std::this_thread::get_id())
        {
            std::cout << "current_tasks_ : " << current_threads_.load() << std::endl;
            if ((current_tasks_.load() > (current_threads_ * 5)) && (current_threads_.load() < max_threads_))
            {
                std::thread *work_thread = new std::thread(&ThreadPool::WorkThread, this);
                {
                    std::lock_guard<std::mutex> lock_g(threads_mutex_);
                    thread_map_.insert(std::pair<std::thread::id, std::thread *>(work_thread->get_id(), work_thread));
                }
                current_threads_.fetch_add(1);
            }
            else if ((current_tasks_.load() < current_threads_.load()) && (current_threads_.load() > init_threads_))
            {
                isquit_.store(true);
                threads_cond_.notify_one();
            }

            std::unique_lock<std::mutex> unique_task(tasks_mutex_);
            if (!tasks_queue_.empty())
            {
                std::lock_guard<std::mutex> idle_lock(idle_mutex_);
                if (!idle_threads_.empty())
                {
                    leader_id_ = idle_threads_.front();
                    threads_cond_.notify_all();
                    idle_threads_.pop();
                }
                task_fun(unique_task);
            }
        }
        //如果当前线程是追随者线程
        else
        {
            std::unique_lock<std::mutex> unique_task(tasks_mutex_);
            if (!tasks_queue_.empty())
            {
                task_fun(unique_task);
            }
            else
            {
                std::unique_lock<std::mutex> unique_idle(idle_mutex_);
                idle_threads_.push(std::this_thread::get_id());
                unique_idle.unlock();
                threads_cond_.wait(unique_task, [this]()
                                   { return stop_.load() || isquit_.load() || !tasks_queue_.empty() || leader_id_ == std::this_thread::get_id(); });

                if (tasks_queue_.empty())
                {
                    unique_task.unlock();
                    isquit_.store(false);
                    break;
                }
                else
                {
                    continue;
                }
            }
        }
    }
    current_threads_.fetch_sub(1);
    std::lock_guard<std::mutex> thread_lock(threads_mutex_);
    std::map<std::thread::id, std::thread *>::iterator it = thread_map_.find(std::this_thread::get_id());
    thread_map_.erase(it);
}

void ThreadPool::ImmediatelyStopAllThreads()
{
    stop_.store(true);
    std::map<std::thread::id, std::thread *>::iterator it = thread_map_.begin();
    while (!thread_map_.empty())
    {

        threads_cond_.notify_all();
    }
}

void ThreadPool::WaitStopAllThreads()
{
    auto is_empty = [this]() -> bool
    {
        std::lock_guard<std::mutex> task_lock(tasks_mutex_);
        if (tasks_queue_.empty())
        {
            return true;
        }
        else
        {
            return false;
        }
    };

    while (!is_empty())
    {
        continue;
    }
    stop_.store(true);
    std::map<std::thread::id, std::thread *>::iterator it = thread_map_.begin();
    while (!thread_map_.empty())
    {
        threads_cond_.notify_all();
    }
}