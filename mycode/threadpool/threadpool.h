//
// Created by chen on 2022/9/30.
//

#ifndef LINUX_HIGH_PERFORMANCE_SERVER_THREADPOOL_H
#define LINUX_HIGH_PERFORMANCE_SERVER_THREADPOOL_H

#include <list>
#include <stdio.h>
#include <exception>
#include <pthread.h>
#include "../locker/locker.h"

template<class T>
class threadpool{
public:
    threadpool(int thread_number = 8, int max_requests = 1000);
    ~threadpool();
    bool append(T *request);

private:
    int m_thread_number;        // 线程池中的线程数
    int m_max_requests;         // 请求队列中允许的最大请求数
    pthread_t *m_threads;       // 工作线程数组
    std::list<T*> m_workqueue;  // 请求队列
    locker m_queue_locker;      // 请求队列互斥锁
    sem m_queuestat;            // 是否有任务需要请求
    bool m_stop;

    static void* worker(void *arg); // 工作线程运行的函数，不断从请求队列中取出任务
    void run_thread();
};

/*
 *      构造函数
 *      初始化thread_number个子线程，然后将它们与主线程分离
 */
template<class T>
threadpool<T>::threadpool(int thread_number, int max_requests):m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(nullptr){
    if(thread_number <= 0 || max_requests <= 0){
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::bad_alloc();
    }

    for(int i = 0; i < thread_number; ++i){
        printf("create the thread %d\n", i);

        /*
         *   注意：pthread_create传入的线程函数必须是静态函数
         *   要在静态函数中使用类的动态成员，有两种方法：
         *   1. 通过类的静态对象来调用。比如单例模式中的静态对象
         *   2. 将类的对象作为参数传给该静态函数，然后在函数中引用这个对象。这里使用第二个方法
         */
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete [] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]) != 0){
            delete [] m_threads;
            throw std::exception();
        }
    }
}

/*
 *      析构函数
 */
template <class T>
threadpool<T>::~threadpool() {
    delete [] m_threads;
    m_stop = true;
}

/*
 *      向任务队列插入任务
 */
template<class T>
bool threadpool<T>::append(T *request) {
    m_queue_locker.lock();
    if(m_workqueue.size() >= m_max_requests){
        m_queue_locker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queue_locker.unlock();
    m_queuestat.post();     // 唤醒一个等待线程
    return true;
}

/*
 *      启动一个工作线程
 */
template <class T>
void *threadpool<T>::worker(void *arg) {
    threadpool<T> *pool = (threadpool<T> *)arg;
    pool->run_thread();
    return pool;
}

/*
 *      工作线程函数
 */
template <class T>
void threadpool<T>::run_thread(){
    while(!m_stop){
        m_queuestat.wait();     // 等待任务队列有任务可以领取
        m_queue_locker.lock();
        if(m_workqueue.empty()){
            m_queue_locker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queue_locker.unlock();
        if(!request){
            continue;
        }
        request->process();
    }
}

#endif //LINUX_HIGH_PERFORMANCE_SERVER_THREADPOOL_H
