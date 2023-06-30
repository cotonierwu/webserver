#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <cstdio>
#include "locker.h"

// 线程池模板类
// T是任务类
template <typename T>
class threadpool
{
public:
    threadpool(int thread_num = 8, int max_requst = 10000);
    ~threadpool();

    bool append(T *request);

private:
    static void *worker(void *arg);
    void run();

private:
    // 线程的数量
    int m_thread_num;
    // 线程数组
    pthread_t *m_threads;
    // 是否结束线程
    bool m_stop;

    // 请求队列中最多允许的等待数量
    int m_max_requst;
    // 请求队列
    std::list<T *> m_workqueue;
    // 互斥锁
    locker m_queuelocker;
    // 信号量
    seme m_queuesem;
};

template <typename T>
threadpool<T>::threadpool(int thread_num, int max_requst) : m_thread_num(thread_num), m_max_requst(max_requst), m_stop(false), m_threads(nullptr)
{
    if (thread_num <= 0 || max_requst <= 0)
    {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_num];
    if (!m_threads)
    {
        throw std::exception();
    }
    // 创建线程，设置线程脱离
    for (int i = 0; i < m_thread_num; i++)
    {
        printf("create the %d thread\n", i);
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requst)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuesem.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
        m_queuesem.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }
        request->process();
    }
}

#endif