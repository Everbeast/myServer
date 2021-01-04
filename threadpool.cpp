#include "threadpool.h"

ThreadPool::ThreadPool(int thread_number, int max_requests):
    m_thread_number(thread_number),m_max_requests(max_requests),
    m_cond(m_mutex),m_stop(false)
{
    if(thread_number <= 0 || thread_number > MAX_THREAD_SIZE){
        std::cout << "thread nummber is invalid, set as default" << DEFAULT_THREAD_SIZE << std::endl;
        thread_number = DEFAULT_THREAD_SIZE;
    }
    if(max_requests <= 0 || max_requests > MAX_QUEUE_SIZE)
    {
        std::cout << "request que size is invalid, set as maximum " << MAX_QUEUE_SIZE << std::endl;
        max_requests = MAX_QUEUE_SIZE;
    }

    m_threads.reserve(thread_number);

    //创建thread_number 个线程 并将它们都设置为脱离线程
    for(int i = 0; i < thread_number; ++i)
    {
        if(pthread_create(&m_threads[i], NULL, worker, this) != 0)
        {
            std::cout << "create thread error" << std::endl;
            throw std::exception();
        }
    }
}

ThreadPool::~ThreadPool()
{
    MutexLockGuard lock(m_mutex);
    m_stop = true;
    m_cond.signal_all();

}


bool ThreadPool::append(std::function<void()> task)
{
    //操作工作队列时一定要加锁， 因为它被所有线程共享
    MutexLockGuard lock(m_mutex);
    if(m_workqueue.size() > m_max_requests)
    {
        std::cout << "too many request" << std::endl;
        return false;
    }
    m_workqueue.emplace(task);
    m_cond.signal();
    return true;
}


void* ThreadPool::worker(void* arg)
{
    std::cout<<"in worker" << std::endl;
    ThreadPool* pool = (ThreadPool*) arg;
    if(pool == nullptr)
        return NULL;
    pool->run();
    return pool;
}

void ThreadPool::run()
{
    std::cout<<"worker ran"<<std::endl;
    while(true)
    {
       std::function<void()> task;
        {
            MutexLockGuard lock(m_mutex);
            while(m_workqueue.empty() && !m_stop)
            {
                m_cond.wait();
            }
            if(m_stop) break;
            task = m_workqueue.front();
            m_workqueue.pop();

        }
        task();
    }
}