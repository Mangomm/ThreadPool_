#ifndef __THREADPOOL__H__
#define __THREADPOOL__H__

#include <mutex>
#include <vector>

#define DEFAULT_TIME 10                 /* 10s检测一次 */
#define MIN_WAIT_TASK_NUM 10			/* 如果queue_size > MIN_WAIT_TASK_NUM 添加新的线程到线程池 */
#define DEFAULT_THREAD_VARY 10          /* 每次创建和销毁线程的个数 */

namespace HCM_NAMESPACE
{
    typedef struct {
        void *(*function)(void *);          /* 函数指针，回调函数 */
        void *arg;                          /* 上面函数的参数 */
    } threadpool_task_t; 


    //定义一个 线程池中的 线程 的结构，以后可能做一些统计之类的 功能扩展，所以引入这么个结构来 代表线程 感觉更方便一些；
    class CThreadPool;
    struct ThreadItem   
    {
        //构造函数
        ThreadItem() {}
        ThreadItem(CThreadPool *pthis):_pThis(pthis), _Handle(0), ifrunning(false){}                             
        //析构函数
        ~ThreadItem(){} 

        CThreadPool *_pThis;                              //记录线程池的指针	
        pthread_t   _Handle;                              //线程句柄
        bool        ifrunning;                            //标记是否正式启动起来，启动起来后，才允许调用StopAll()来释放。暂未使用
    };

    class CThreadPool
    {
        public:
            CThreadPool();
            ~CThreadPool();

            bool threadpool_create(int min_thr_num, int max_thr_num, int queue_max_size);
            int threadpool_add(void*(*function)(void *arg), void *arg);
            int threadpool_destroy();

            int threadpool_all_threadnum();
            int threadpool_busy_threadnum();
            int is_thread_alive(pthread_t tid);
        private:
            int threadpool_free_create(bool isInitMC);

        public:
            pthread_mutex_t m_lock;               /* 锁住本接类，由于MyLcok类不方便随时解锁，所以还是用回pthread_mutex_t */
            pthread_mutex_t m_thread_counter;     /* 记录忙状态线程个数de琐 -- busy_thr_num */

            std::vector<ThreadItem*> m_threads;   /* 线程数组，无需锁住该数组队列的锁，因为队列属于本对象，m_lock已经锁住 */
            ThreadItem m_adjust;                  /* 调整线程 */
            pthread_mutex_t m_gar;                /* 垃圾回收队列锁 */
            std::vector<ThreadItem*> m_garbage;   /* 回收由调整线程退出的线程资源. */
                                                  
            pthread_cond_t m_queue_not_full;      /* 当任务队列满时，添加任务的线程阻塞，等待此条件变量 */
            pthread_cond_t m_queue_not_empty;     /* 任务队列里不为空时，通知等待任务的线程 */

            int m_min_thr_num;                    /* 最小线程数 */
            int m_max_thr_num;                    /* 最大线程数 */
            int m_live_thr_num;                   /* 当前存活线程个数 */
            int m_busy_thr_num;                   /* 忙状态线程个数 */
            int m_wait_exit_thr_num;              /* 要销毁的线程个数 */

            threadpool_task_t *m_task_queue;      /* 任务队列 */

            int m_queue_front;                    /* task_queue队头下标 */
            int m_queue_rear;                     /* task_queue队尾下标 */
            int m_queue_size;                     /* task_queue队中实际任务数 */
            int m_queue_max_size;                 /* task_queue队列可容纳任务数上限 */

            int m_shutdown;                       /* 标志位，线程池使用状态，true代表将要关闭线程池，false代表不关 */
    };
}


#endif