#include <iostream>
#include <thread>
#include <set>

#include "threadpool.h"

int shuchu(int x)
{
    std::cout << "thread id : " << std::this_thread::get_id() << std::endl;
    return x;
}

class Test
{
private:
    /* data */
public:
    Test(/* args */);
    ~Test();
    int shuchu(int x)
    {
        return x;
    }
};

Test::Test(/* args */)
{
}

Test::~Test()
{
}

int main(void)
{
    Test test;
    ThreadPool *threadpool = new ThreadPool();
    if (threadpool->ThreadPoolInit(4, 3) != 0)
    {
        return -1;
    }

    for (int i = 0; i < 100; i++)
    {
        threadpool->AddTask(shuchu, 4);
    }

    std::future<int> fut = threadpool->AddTask(shuchu, 22);
    std::future<int> fut2 = threadpool->AddTask([&test](){
        return test.shuchu(33);
    });

    std::cout << "get: " << fut.get() << std::endl;
    std::cout << "get: " << fut2.get() << std::endl;

    while (threadpool->GetTaskCount() != 0)
    {
        continue;
    }

    threadpool->StopAllThreads();
    return 0;
}