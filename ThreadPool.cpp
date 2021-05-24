#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include "ThreadPool.h"

namespace HCM_NAMESPACE
{
    CThreadPool::CThreadPool(){}
    CThreadPool::~CThreadPool(){}

    //后面可考虑写成labmda表达式，ok.后续需要添加管理线程相关的代码
    void *threadpool_thread(void *threadpool)
    {
        if(NULL == threadpool){
            return (void*)-1;
        }

        CThreadPool *pool = (CThreadPool*)threadpool;
        threadpool_task_t task;

        while (true)
        {
            pthread_mutex_lock(&(pool->m_lock));

            while(pool->m_queue_size <= 0 && pool->m_shutdown == false)
            {
                printf("thread 0x%x is waiting\n", (unsigned int)pthread_self());
                pthread_cond_wait(&(pool->m_queue_not_empty), &(pool->m_lock));
            }

            //到这里可能是任务不为空或者线程池被关闭

            //1 若线程池被关闭，则退出该线程.这里是线程统一退出的接口,有多少个exiting就有多少个线程退出
            if(pool->m_shutdown == true){
                pthread_mutex_unlock(&(pool->m_lock));
                printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
                pthread_exit(NULL);     /* 线程自行结束,注意pthread_exit退出的与return一样，仍需调用join回收资源.这里未添加调整线程也能被回收，
                                            因为退出时m_live_thr_num没有减减，所以destory也可以join */
            }

            //2 否则执行任务
            /*从任务队列里获取任务, 是一个出队操作*/
            task.function = pool->m_task_queue[pool->m_queue_front].function;
            task.arg = pool->m_task_queue[pool->m_queue_front].arg;

            /* 更新队头出队，模拟环形队列 */
            pool->m_queue_front = (pool->m_queue_front + 1) % pool->m_queue_max_size;
            /*更新任务数*/
            pool->m_queue_size--;       

            /* 广播通知可以有新的任务添加进来 */
            pthread_cond_broadcast(&(pool->m_queue_not_full));

            /*任务取出后，立即将 线程池琐 释放*/
            pthread_mutex_unlock(&(pool->m_lock));

            /*执行任务*/ 
            //printf("thread 0x%x start working\n", (unsigned int)pthread_self());
            pthread_mutex_lock(&(pool->m_thread_counter));                          /*忙状态线程数变量琐*/
            pool->m_busy_thr_num++;                                                 /*忙状态线程数+1*/
            pthread_mutex_unlock(&(pool->m_thread_counter));
            (*(task.function))(task.arg);                                           /*执行回调函数任务*/
            //task.function(task.arg);                                              /*执行回调函数任务*/

            /*任务结束处理*/ 
            //printf("thread 0x%x end working\n", (unsigned int)pthread_self());
            pthread_mutex_lock(&(pool->m_thread_counter));
            pool->m_busy_thr_num--;                                                 /*处理掉一个任务，忙状态数线程数-1*/
            pthread_mutex_unlock(&(pool->m_thread_counter));
        }
        
        pthread_exit(NULL);//可以不要，因为上面没有break。但存在意外退出while，所以最好回收
    }

    //ok，后续需要添加管理线程相关的代码
    bool CThreadPool::threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size)
    {
        if (min_thr_num <= 0
            || min_thr_num > max_thr_num
            || queue_max_size <= 0) 
        {
            return false;
        }

        do
        {
            m_min_thr_num = min_thr_num;
            m_max_thr_num = max_thr_num;

            m_live_thr_num = min_thr_num;               /* 活着的线程数 初值=最小线程数 */
            m_busy_thr_num = 0;

            m_queue_front = 0;
            m_queue_rear = 0;
            m_queue_size = 0;
            m_queue_max_size = queue_max_size;

            m_shutdown = false;

            m_task_queue = NULL;
            m_task_queue = (threadpool_task_t *)new threadpool_task_t[queue_max_size]();  /* 后面加()代表初始化自动赋0，这样可以不调用memset */
            if(NULL == m_task_queue){
                std::cout<<"new m_task_queue failed."<<std::endl;
                break;
            }
            //memset(m_task_queue, 0x00, sizeof(m_task_queue) * queue_max_size);

            m_threads = NULL;
            m_threads = (pthread_t *)new pthread_t[max_thr_num]();
            if(NULL == m_threads){
                std::cout<<"new m_threads failed."<<std::endl;
                break;
            }
            //memset(m_threads, 0x00, sizeof(pthread_t) * max_thr_num);
            
            /* 初始化互斥琐、条件变量 */
            if (pthread_mutex_init(&(m_lock), NULL) != 0
                || pthread_mutex_init(&(m_thread_counter), NULL) != 0
                || pthread_cond_init(&(m_queue_not_empty), NULL) != 0
                || pthread_cond_init(&(m_queue_not_full), NULL) != 0)
            {
                std::cout<<"init the lock or cond fail"<<std::endl;
                break;
            }

            int i = 0;
            for(i = 0; i < min_thr_num; i++){
                pthread_create(&m_threads[i], NULL, threadpool_thread, (void*)this);
                printf("start thread 0x%x...\n", (unsigned int)m_threads[i]);
            }
            
            return true;
        }while(0);

        threadpool_free();

        return false;
    }

    //ok,可以无需再处理
    int CThreadPool::threadpool_add(void*(*function)(void *arg), void *arg)
    {
        pthread_mutex_lock(&m_lock);

        while(m_shutdown == false && m_queue_size >= m_queue_max_size)
        {
            std::cout<<"Task too much, thread bolcking"<<std::endl;
            pthread_cond_wait(&m_queue_not_full, &m_lock);
        }

        if (m_shutdown) {
            pthread_cond_broadcast(&(m_queue_not_empty));//唤醒多个线程,让阻塞在空任务的线程唤醒。这句可以不写，但最好写，以让每个任务都能让阻塞的线程被唤醒。
            pthread_mutex_unlock(&m_lock);
            return 0;//与上面的pthread_cond_broadcast可以不写，通过下面的操作进行唤醒，不过最好还是写
        }

        /* 清空 工作线程 调用的回调函数 的参数arg */
        if (m_task_queue[m_queue_rear].arg != NULL) {
            //free(pool->task_queue[pool->queue_rear].arg);//不能释放临时变量
            m_task_queue[m_queue_rear].arg = NULL;
        }

        m_task_queue[m_queue_rear].function = function;
        m_task_queue[m_queue_rear].arg = arg;
        m_queue_rear = (m_queue_rear + 1) % m_queue_max_size;       /* 队尾指针移动, 模拟环形 */
        m_queue_size++;

        /*添加完任务后，队列不为空，唤醒线程池中 等待处理任务的线程*/
        pthread_cond_signal(&m_queue_not_empty);//至少唤醒一个线程。
        pthread_mutex_unlock(&m_lock);

        return 0;
    }

    //ok，后续需要添加调整线程的代码
    int CThreadPool::threadpool_destroy()
    {
        if(m_shutdown)
        {
            return 0;
        }

        m_shutdown = true;

        int i;
        for (i = 0; i < m_live_thr_num; i++) {
            /*通知所有的空闲线程,避免仍有运行的线程无法得到通知而阻塞*/
            pthread_cond_broadcast(&m_queue_not_empty);
        }
        for (i = 0; i < m_live_thr_num; i++) {
            /*回收正在运行的线程.注意：即使调用了pthread_exit退出，也算正在运行，因为m_live_thr_num并未减1.这
              样才能回收掉子进程残留的资源.pthread_exit与return退出的线程均需用join回收资源.
            */
            pthread_join(m_threads[i], NULL);
        }
        threadpool_free();

        return 0;
    }

    //ok,与调整线程无关
    int CThreadPool::threadpool_free()
    {
        if (m_task_queue) {
            delete [] m_task_queue;
            m_task_queue = NULL;
        }

        if (m_threads) {
            delete [] m_threads;
            m_threads = NULL;

            pthread_mutex_lock(&m_lock);//C语言必须先上锁再destory锁，否则别人(threadpool_thread)还再使用的话就会出现问题
            pthread_mutex_destroy(&m_lock);
            pthread_mutex_lock(&m_thread_counter);
            pthread_mutex_destroy(&m_thread_counter);

            //即使没有调用pthread_cond_init也不会崩溃，正常现象.放进if(m_threads)中或者外面处理也行
            //但这里放if里更好，因为m_threads为空，创建时必定不会初始化到锁和条件变量
            pthread_cond_destroy(&m_queue_not_empty);
            pthread_cond_destroy(&m_queue_not_full);
        }

        return 0;
    }

    //获取线程池里存活的线程数,ok
    int CThreadPool::threadpool_all_threadnum()
    {
        int all_threadnum = -1;
        pthread_mutex_lock(&m_lock);
        all_threadnum = m_live_thr_num;
        pthread_mutex_unlock(&m_lock);

        return all_threadnum;
    }

    //获取忙线程数,ok
    int CThreadPool::threadpool_busy_threadnum()
    {
        int busy_threadnum = -1;
        pthread_mutex_lock(&m_thread_counter);
        busy_threadnum = m_busy_thr_num;
        pthread_mutex_unlock(&m_thread_counter);
        return busy_threadnum;
    }

    //测试某个线程释放存活,ok
    int CThreadPool::is_thread_alive(pthread_t tid)
    {
        int kill_rc = pthread_kill(tid, 0);     //发0号信号，测试线程是否存活
        if (kill_rc == ESRCH) {
            return false;
        }

        return true;
    }

}
