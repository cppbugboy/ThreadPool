#ifndef _THREADPOOL_
#define _THREADPOOL_

#include <thread>
#include <mutex>
#include <functional>
#include <map>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <future>

class ThreadPool
{
private:
    unsigned int max_threads_ = 0;  //最大线程数
    unsigned int max_tasks_ = 0;    //最大任务数
    unsigned int init_threads_ = 0; //初始化线程数
    std::thread::id leader_id_;     //领导者线程id

    std::map<std::thread::id, std::thread *> thread_map_;   //线程容器
    std::mutex threads_mutex_;                              //线程锁

    std::atomic_uint current_threads_;                      //当前线程数
    std::atomic_uint current_tasks_;                        //当前任务数

    std::condition_variable threads_cond_;                  //线程条件变量锁

    std::queue<std::function<void()>> tasks_queue_;         //任务队列
    std::mutex tasks_mutex_;                                //任务队列锁

    std::atomic_bool isquit_;                               //线程是否要退出
    std::atomic_bool stop_;                                 //所有线程是否停止

    std::queue<std::thread::id> idle_threads_;              //追随者线程容器
    std::mutex idle_mutex_;                                 //追随者线程容器锁

    /*
     *功能：线程运行函数
    */
    void WorkThread();

public:
    ThreadPool();
    ~ThreadPool();

    /*
     *功能：获得线程池实例
     *返回：创建的线程池实例
    */
   static ThreadPool* Instance();

    /*
     *功能：线程池初始化
     *输入：最大线程数、初始化线程数、最大任务数
    */
    void ThreadPoolInit(const unsigned int max_threads, const unsigned int init_threads, const unsigned int max_tasks);

    /*
     *功能：强制结束所有线程
    */
    void ImmediatelyStopAllThreads();

    /*
     *等待县城池任务完成后结束所有线程
    */
    void WaitStopAllThreads();

    /*
     *功能：提交任务到任务队列
    */
    template <typename F, typename... Args>
    auto AddTask(F &&f, Args &&...args) -> std::future<decltype(f(args...))>
    {
        using retype = decltype(f(args...));
        // using rt = typename std::result_of<F(Args...)>::type;
        auto task = std::make_shared<std::packaged_task<retype()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<retype> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock_guard(tasks_mutex_);
            tasks_queue_.emplace([task]()
                                 { (*task)(); });
        }

        threads_cond_.notify_one();
        current_tasks_.fetch_add(1);
        return result;
    }
};

#endif