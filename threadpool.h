
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <iostream>
#include <exception>
#include <vector>
#include <pthread.h>
#include <functional>
#include <queue>
#include "locker.h"


const int DEFAULT_THREAD_SIZE = 8;
const int MAX_THREAD_SIZE = 1024;
const int MAX_QUEUE_SIZE = 10000;

class ThreadPool
{
public:
    ThreadPool(int thread_number = DEFAULT_THREAD_SIZE, int max_requests = MAX_QUEUE_SIZE);
    ~ThreadPool();
    //往请求队列中添加任务
    bool append(std::function<void()> task);
private:
    static void* worker(void* arg);
    void run();

private:
    MutexLock m_mutex; //保护请求队列的互斥锁
    Condition m_cond; //是否有任务需要处理

    int m_thread_number; //线程池的线程数
    int m_max_requests; //请求队列中允许的最大请求数
    std::vector<pthread_t> m_threads; //描述线程池的数组，大小为m_thread_number
    std::queue<std::function<void()>> m_workqueue; //请求队列

    bool m_stop; //是否结束线程
};

#endif