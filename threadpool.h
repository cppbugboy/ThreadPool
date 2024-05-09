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
    unsigned int init_threads_ = 0; //初始化线程数
    std::thread::id leader_id_;     //领导者线程id

    std::map<std::thread::id, std::thread *> thread_map_;   //线程容器
    std::mutex threads_mutex_;                              //线程锁

    std::atomic_uint count_threads_;                      //当前线程数
    std::atomic_uint count_tasks_;                        //当前任务数

    std::condition_variable task_cond_;                  //线程条件变量锁

    std::queue<std::function<void()>> tasks_queue_;         //任务队列
    std::mutex tasks_mutex_;                                //任务队列锁

    std::atomic_bool quit_;                               //线程是否要退出
    std::atomic_bool stop_;                                 //所有线程是否停止

    /*
     *功能：线程运行函数
    */
    void WorkThread();

public:
    ThreadPool();
    ~ThreadPool();

    /*
     * explain: 获取线程池单例实例
     * param:
     * return: 创建的线程池实例指针
    */
   static ThreadPool* GetInstance();

    /*
     * explain: 线程池初始化
     * param:
     *      [IN] max_threads: 最大线程数
     *      [IN] init_threads: 初始线程数，默认 1
     * return:
    */
    int ThreadPoolInit(const unsigned int max_threads, const unsigned int init_threads);


    const unsigned int GetTaskCount();
    /*
     * explain: 结束所有线程
     * param:
     * return:
    */
    void StopAllThreads();

    /*
     * explain: 添加任务
     * param:
     *      [IN] f: 函数
     *      [IN] args: 参数
     * return:
     *      任务返回值的future
    */
    template <typename F, typename... Args>
    auto AddTask(F &&f, Args &&...args) -> std::future<decltype(f(args...))>
    {
        using retype = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<retype()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<retype> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock_guard(tasks_mutex_);
            tasks_queue_.emplace([task]()
                                 { (*task)(); });
        }

        task_cond_.notify_one();
        count_tasks_.fetch_add(1);
        return result;
    }
};

#endif