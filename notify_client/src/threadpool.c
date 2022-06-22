#include "threadpool.h"
#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "notify_client_common.h"

/*
int pthread_mutex_init(pthread_mutex_t * mutex ,pthread_mutexattr_t * attr);
int pthread_mutex_destroy(pthread_mutex_t * mutex);
int pthread_mutex_lock(pthread_mutex_t * mutex ); 
//阻塞式
int pthread_mutex_unlock(pthread_mutex_t * mutex );
int pthread_mutex_trylock(pthread_mutex_t * mutex );
//非阻塞式
int pthread_mutex_timedlock(pthread_mutex_t mutex, const struct timespec *tsptr);

#include<pthread.h>
int pthread_cond_init(pthread_cond_t *restrict cond, constpthread_condattr_t *restrict attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex);
//阻塞等待条件变量
int pthread_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, const struct timespec *restrict abstime);
//超时等待
int pthread_cond_signal(pthread_cond_t *cond);
 //唤醒一个或者多个等待的线程
 int pthread_cond_broadcast(pthread_cond_t *cond);
 //唤醒所有的等待的线程条件变量通过允许线程阻塞和等待另一个线程发送信号，可以解决消费者和生产者的关系
*/

#define POOL_MAX_SIZE 5
#define TASK_MAX_CNT 60
#define KILL_PRHREAD_RETRY_MAX_TIMES 10
#define KILL_PRHREAD_RETRY_SLEEP_TIME (50 * 1000)

#define THREAD_LOG_ERROR(fmt, ...) printf("{threadPool} [ERROR] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define THREAD_LOG_WARN(fmt, ...) printf("{threadPool} [WARN] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define THREAD_LOG_INFO(fmt, ...) printf("{threadPool} [INFO] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)
#define THREAD_LOG_DEBUG(fmt, ...) printf("{threadPool} [DEBUG] <%s() %d> "fmt"\n", __func__, __LINE__, ##__VA_ARGS__)

struct ThreadWork {
    void (*func)(void *arg);
    void *arg;
    struct ThreadWork *next;
};

struct ThreadPool {
    int maxPoolSize;
    pthread_t *threadId;
    pthread_mutex_t queueLock;
    pthread_cond_t wait;
    struct ThreadWork *queueHead;
    int off;
    int taskCnt;
};

static int g_threadRun = 0;
static struct ThreadPool *g_threadPool = NULL;

static void *ProcessProgram(void *UNUSED(arg))
{
    while (g_threadRun == 1) {
        pthread_mutex_lock(&g_threadPool->queueLock);
        while (g_threadPool->taskCnt == 0 && g_threadPool->off == 0) {
            pthread_cond_wait(&g_threadPool->wait, &g_threadPool->queueLock);
        }
        if (g_threadPool->off == 1 || g_threadPool->taskCnt == 0 || g_threadPool->queueHead == NULL) {
            pthread_mutex_unlock(&g_threadPool->queueLock);
            break;
        }
        struct ThreadWork *work = g_threadPool->queueHead;
        g_threadPool->taskCnt -= 1;
        g_threadPool->queueHead = g_threadPool->queueHead->next;
        pthread_mutex_unlock(&g_threadPool->queueLock);
        work->func(work->arg);
        free(work);
        work = NULL;
    }
    // THREAD_LOG_INFO("task [%u] exit", (unsigend int)pthread_self());
    return NULL;
}

void DestroyThreadPool(void)
{
    if (g_threadPool == NULL) {
        THREAD_LOG_ERROR("pool has been destroy");
        return;
    }
    g_threadRun = 0;
    pthread_mutex_lock(&g_threadPool->queueLock);
    struct ThreadWork *head = g_threadPool->queueHead;
    while (head != NULL) {
        struct ThreadWork *temp = head;
        head = head->next;
        free(temp);
        temp = NULL;
    }
    g_threadPool->queueHead = NULL;
    g_threadPool->off = 1;
    pthread_mutex_unlock(&g_threadPool->queueLock);
    // 唤醒所有线程，马上要退出了
    pthread_cond_broadcast(&g_threadPool->wait);
    int index = 0;
    int retry = 0;
    while (index < g_threadPool->maxPoolSize && retry < KILL_PRHREAD_RETRY_MAX_TIMES) {
        if (pthread_kill(g_threadPool->threadId[index], 0) == ESRCH) {
            index++;
            retry = 0;
        } else {
            retry++;
            usleep(KILL_PRHREAD_RETRY_SLEEP_TIME); // 50ms重试
            THREAD_LOG_ERROR("pthread_kill tid [%u] faild retry %d", (unsigned int)g_threadPool->threadId[index], retry);
        }
    }
    if (retry >= KILL_PRHREAD_RETRY_MAX_TIMES) {
        THREAD_LOG_ERROR("pthread_kill faild time %d", retry);
    }
    free(g_threadPool->threadId);
    g_threadPool->threadId = NULL;
    pthread_cond_destroy(&g_threadPool->wait);
    pthread_mutex_destroy(&g_threadPool->queueLock);
    free(g_threadPool);
    g_threadPool = NULL;
}

int InitThreadPool(int maxPoolSize)
{
    if (g_threadPool != NULL) {
        THREAD_LOG_ERROR("pool has been initialized");
        return -1;
    }
    if (maxPoolSize > POOL_MAX_SIZE || maxPoolSize < 0) {
        THREAD_LOG_ERROR("input pool %d is invalid", maxPoolSize);
        return -1;
    }
    g_threadPool = (struct ThreadPool *)malloc(sizeof(struct ThreadPool));
    if (g_threadPool == NULL) {
        THREAD_LOG_ERROR("malloc Pool faild ret %d", (int)errno);
        return -1;
    }
    memset((void *)g_threadPool, 0, sizeof(struct ThreadPool));
    if (maxPoolSize == 0) {
        g_threadPool->maxPoolSize = POOL_MAX_SIZE;
    } else {
        g_threadPool->maxPoolSize = maxPoolSize;
    }
    g_threadPool->threadId = (pthread_t *)malloc(sizeof(pthread_t));
    if (g_threadPool->threadId == NULL) {
        THREAD_LOG_ERROR("malloc threadId faild ret %d", (int)errno);
        free(g_threadPool);
        g_threadPool = NULL;
        return -1;
    }
    pthread_mutex_init(&g_threadPool->queueLock, NULL);
    pthread_cond_init(&g_threadPool->wait, NULL);
    g_threadRun = 1;
    for (int i = 0; i < g_threadPool->maxPoolSize; i++) {
        if (pthread_create(&g_threadPool->threadId[i], NULL, (void *)ProcessProgram, NULL) != 0) {
            THREAD_LOG_ERROR("create thread faild ret %d", (int)errno);
            g_threadRun = 0;
            DestroyThreadPool();
            return -1;
        }
    }
    THREAD_LOG_INFO("create thread pool size %d", g_threadPool->maxPoolSize);
    return 0;
}

int AddTaskInThreadPool(void (*func)(void *arg), void *arg)
{
    if (g_threadRun == 0 || g_threadPool == NULL) {
        THREAD_LOG_ERROR("pool can not run");
        return -1;
    }
    if (func == NULL) {
        THREAD_LOG_ERROR("parameter invaild");
        return -1;
    }
    pthread_mutex_lock(&g_threadPool->queueLock);
    if (g_threadPool->taskCnt > TASK_MAX_CNT) {
        THREAD_LOG_ERROR("had too many task wait to process cant not push new");
        pthread_mutex_unlock(&g_threadPool->queueLock);
        return -1;
    }
    struct ThreadWork *node = (struct ThreadWork *)malloc(sizeof(struct ThreadWork));
    node->next = NULL;
    node->func = func;
    node->arg = arg;
    if (g_threadPool->taskCnt != 0) {
        struct ThreadWork *temp = g_threadPool->queueHead;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = node;
    } else {
        g_threadPool->queueHead = node;
    }
    g_threadPool->taskCnt += 1;
    pthread_mutex_unlock(&g_threadPool->queueLock);
    pthread_cond_signal(&g_threadPool->wait);
    return 0;
}

bool IsThreadPoolInit(void)
{
    if (g_threadRun == 0 || g_threadPool == NULL) {
        return false;
    }
    return true;
}