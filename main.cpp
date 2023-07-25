#include <iostream>
#include <thread>
#include <unistd.h>
#include <set>

#include "threadpool.h"

std::set<std::thread::id> t_id_set;

void shuchu()
{
    t_id_set.insert(std::this_thread::get_id());
    usleep(100000);
    std::cout << "thread id : " << std::this_thread::get_id() << std::endl;
}

class Test
{
private:
    /* data */
public:
    Test(/* args */);
    ~Test();
    void shuchu()
    {
        t_id_set.insert(std::this_thread::get_id());
        usleep(1000);
        // std::cout << "thread id : " << std::this_thread::get_id() << std::endl;
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
    threadpool->ThreadPoolInit(6, 2, 100);
    for (int i = 0; i < 100; i++)
    {
        threadpool->AddTask([&test](){test.shuchu();});
    }

    threadpool->WaitStopAllThreads();
    std::cout << "t_id_set size : " << std::to_string(t_id_set.size()) << std::endl;
    return 0;
}