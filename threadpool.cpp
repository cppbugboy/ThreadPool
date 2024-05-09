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

ThreadPool *ThreadPool::GetInstance()
{
    static ThreadPool *threadpool = new ThreadPool();
    return threadpool;
}

int ThreadPool::ThreadPoolInit(const unsigned int max_threads, const unsigned int init_threads)
{
    if (init_threads < 1)
    {
        return -1;
    }

    if (init_threads > max_threads)
    {
        return -2;
    }

    max_threads_ = max_threads;
    init_threads_ = init_threads;

    count_threads_.store(0);
    count_tasks_.store(0);
    quit_.store(false);
    stop_.store(false);

    for (int i = 0; i < init_threads; i++)
    {
        std::thread *work_thread = new std::thread(&ThreadPool::WorkThread, this);
        {
            std::lock_guard<std::mutex> lock_guard(threads_mutex_);
            thread_map_.insert(std::pair<std::thread::id, std::thread *>(work_thread->get_id(), work_thread));
        }
        count_threads_.fetch_add(1);
    }
    
    leader_id_ = thread_map_.begin()->first;
    return 0;
}

void ThreadPool::WorkThread()
{
    auto task_func = [this](std::unique_lock<std::mutex> &unique_lock)
    {
        std::function<void()> task = std::move(tasks_queue_.front());
        tasks_queue_.pop();
        unique_lock.unlock();
        count_tasks_.fetch_sub(1);
        task();
    };

    while (true)
    {
        if (stop_.load() || quit_.load())
        {
            break;
        }

        std::unique_lock<std::mutex> unique_task(tasks_mutex_);
        if (!tasks_queue_.empty())
        {
            task_func(unique_task);
        }
        else
        {
            task_cond_.wait(unique_task, [this]()
            { return stop_.load() || quit_.load() || !tasks_queue_.empty();});
        }

        //如果当前线程是领导者线程
        if (leader_id_ == std::this_thread::get_id())
        {
            if ((count_tasks_.load() > (count_threads_ * 100)) && (count_threads_.load() < max_threads_))
            {
                std::thread *work_thread = new std::thread(&ThreadPool::WorkThread, this);
                {
                    std::lock_guard<std::mutex> lock_guard(threads_mutex_);
                    thread_map_.insert(std::pair<std::thread::id, std::thread *>(work_thread->get_id(), work_thread));
                }
                count_threads_.fetch_add(1);
            }
            else if ((count_tasks_.load() < count_threads_.load()) && (count_threads_.load() > init_threads_))
            {
                quit_.store(true);
                task_cond_.notify_one();
            }
        }
    }
    count_threads_.fetch_sub(1);
    std::lock_guard<std::mutex> thread_lock(threads_mutex_);
    thread_map_.erase(std::this_thread::get_id());
}

void ThreadPool::StopAllThreads()
{
    stop_.store(true);
    std::map<std::thread::id, std::thread *>::iterator it = thread_map_.begin();
    while (!thread_map_.empty())
    {
        task_cond_.notify_all();
    }
}

const unsigned int ThreadPool::GetTaskCount()
{
    return count_tasks_.load();
}
