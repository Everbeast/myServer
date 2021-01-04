#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>

#include "noncopyable.h"



//互斥锁的类
class MutexLock : noncopyable{
public:
//创建、初始化互斥锁
    MutexLock()
    {
        if(pthread_mutex_init(&m_mutex, NULL) != 0){
            throw std::exception();
        }
    }
    //销毁互斥锁
    ~MutexLock()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    //获取锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    //释放锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t* getPhtreadMutex(){
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};


class MutexLockGuard : noncopyable{
public:
    explicit MutexLockGuard(MutexLock& mutex): m_mutex(mutex){
        m_mutex.lock();
    };

    ~MutexLockGuard(){
        m_mutex.unlock();
    }

private:
    MutexLock& m_mutex;
};
//引用类型的成员变量的初始化问题,它不能直接在构造函数里初始化，
//必须用到初始化列表，且形参也必须是引用类型。
//凡是有引用类型的成员变量的类，不能有缺省构造函数。
//原因是引用类型的成员变量必须在类构造时进行初始化。


//explicti 防止类构造函数的隐式自动转换. (其他变量转换为类的变量)
//explicit只对有一个参数的构造函数有效，或者对除第一个参数其他参数都有默认参数有效

//封装条件变量的类
class Condition : noncopyable{
public:
    //创建并初始化条件变量
    explicit Condition(MutexLock& mutex): m_mutex(mutex){
        if(pthread_cond_init(&m_cond, NULL) != 0){
            throw std::exception();
        }
    }

    //销毁条件变量
    ~Condition()
    {
        pthread_cond_destroy(&m_cond);
    }

    //等待条件变量
    bool wait()
    {
        return pthread_cond_wait(&m_cond, m_mutex.getPhtreadMutex()) == 0;
    }

    //唤醒等待条件变量的线程
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }

    bool signal_all(){
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    MutexLock& m_mutex;
    pthread_cond_t m_cond;
};

#endif