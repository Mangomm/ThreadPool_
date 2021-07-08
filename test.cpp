#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "ThreadPool.h"

using namespace std;
using namespace HCM_NAMESPACE;


bool bExit = 0;

void signal_ctrlc(int sig)
{
//	std::cout<<"get signal:"<<sig<<std::endl;
	bExit = 1;
}

//测试未初始化的条件变量直接被销毁的结果：
//正常，即使pthread_cond_t是未初始化,程序也不会崩溃。
void test01()
{
    pthread_cond_t m_queue_not_full; 
    pthread_cond_destroy(&m_queue_not_full);
    cout<<"hh"<<endl;
}

//测试当一个线程销毁到锁后，另一个线程再上锁解锁是否会崩溃
//结果：正常，不会崩溃。所以在调用create的过程中，立马调用destory应该不会出现问题(假设create返回但部分线程还在创建中，后面可以用代码优化)。
void test02()
{
    pthread_mutex_t m;
    pthread_mutex_init(&m, NULL);

    pthread_mutex_lock(&m);
    pthread_mutex_unlock(&m);//模拟一个线程执行任务后释放锁

    pthread_mutex_lock(&m);//模拟此时主线程刚刚好调用释放线程池函数，拿到锁后释放锁
    pthread_mutex_destroy(&m);

    cout<<"hhfasdasad"<<endl;
    pthread_mutex_lock(&m);//模拟另一个线程在锁被销毁后，再次上锁解锁。注意：由于锁被释放，所以这里并不会造成死锁，结果可以看到"hh"输出.
    pthread_mutex_unlock(&m);

    cout<<"hh"<<endl;

}

//测试回收一个不存在的线程.
//结果：没问题。因为destory时，在m_threads数组中，存活的线程tid并不一定连续排列，当第一次创建满线程时，m_threads中没有0了，只能依靠alive去判断是否能创建线程，
//这样就存在中间和末尾的线程可能是不活的，如果只按照m_live_nums去回收，那么最后的部分线程就无法回收.所以需要遍历m_max_thr_num。
void test03()
{
	pthread_t tid = 0;

	pthread_join(tid, NULL);

	std::cout << "hh ok " << std::endl;
}

/* 线程池中的线程，模拟处理业务 */
void *process(void *arg)
{
	printf("thread working on task %d\n ", *(int *)arg);
    sleep(1);
	printf("task %d is end\n", *(int *)arg);

	return NULL;
}

int main(){

    CThreadPool pool;													/*在C++使用时，一般作为全局对象放
																		  在global.h声明，然后在main函数中定义*/

	signal(SIGINT, signal_ctrlc);

	//1 先创建线程池
	bool ret = pool.threadpool_create(3, 15, 100);								/*创建线程池*/
	if(ret == false)
	{
		printf("pool inited failed\n");
		return -1;
	}
	printf("pool inited Ok\n");
	sleep(1);


	//2 模拟客户端请求的任务
	//注意添加任务时num数组大小也要改变，否则会出现未知错误
	int num[300], i;
	int r;
	srand(time(NULL));
Task:
	r = rand() % 300;//模拟随机任务数
	for (i = 0; i < r; i++) {
		num[i] = i;
		printf("add task %d\n", i);
		pool.threadpool_add(process, (void*)&num[i]);
	}

	printf("添加完一次任务,任务数=%d, 即0 ~ %d\n", r, r - 1);

	if(bExit){
		goto EXIT;
	}	
    sleep(2);
	goto Task;//模拟线程池一直有任务.测试2个小时，稳定且数据正确,若想测试threadpool_destroy，可以注释掉*/

	//sleep(15);
	//goto Task;//模拟线程池任务为空或者不为空的情况(因为睡15s后任务基本执行完，并基本可以赶上管理线程的10s一次定时检测).更加方便测试管理线程的代码.测试2个小时左右，稳定且数据正确，虚拟内存也不再增加。*/

EXIT:
	int busyNum = pool.threadpool_busy_threadnum();
	std::cout << "BusyNum = " << busyNum << std::endl;
	int allThrNum = pool.threadpool_all_threadnum();
	std::cout << "allThrNum = " << allThrNum << std::endl;

	//3 等子线程完成任务	    
    sleep(5);

	std::cout << "main sleep ok " << std::endl;

	//4 销毁线程池
	pool.threadpool_destroy();

	while(1)
	{
		printf("please see Virt in top.\n");//回收线程后，测试查看top命令的虚拟内存的大小.
		sleep(3);
	};

	std::cout << "main end " << std::endl;

    return 0;
}