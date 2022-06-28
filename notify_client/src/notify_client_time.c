
#include "notify_client_time.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include "notify_client_send.h"
#include "notify_client_common.h"

/*
 * 创建一个按时间递增的队列，新的时间插入到队列尾，唤醒timelistener
 * 当队列为空的时候time out 线程休眠
 * 当队列不为空时候每2秒比较一下当前时间和队列头时间，如果大于超时时间则唤醒sendnotify 函数阻塞的线程处理超时业务
 */
#define TIMEOUTSIZE 5 /* 五秒超时 */

struct NotifyTimeOutNode {
    unsigned int seqNum;
    time_t tvSec;
    struct NotifyTimeOutNode *next;
};

struct NotifyTimeOutList {
    struct NotifyTimeOutNode *queueHead;
    unsigned int cnt;
};

static int g_timeListenerRun;
static struct NotifyTimeOutList *g_timeListener = NULL;
static pthread_mutex_t g_timerLock;
static pthread_cond_t g_timerWorkWaite;

bool IsSeqNumTimeOut(unsigned int seqNum)
{
    int ret = -1;
    do {
        pthread_mutex_lock(&g_timerLock);
        if (g_timeListener->queueHead == NULL || g_timeListener->cnt == 0) {
            NOTIFY_LOG_ERROR("TimeoutList status error");
            break;
        }
        struct NotifyTimeOutNode *node = g_timeListener->queueHead;
        while (node != NULL && node->seqNum != seqNum) node = node->next;
        if (node == NULL) {
            NOTIFY_LOG_ERROR("ant not find seqNum %u in time out list", seqNum);
            break;
        }
        struct timeval t;
        gettimeofday(&t, NULL);
        if (t.tv_sec >= node->tvSec) {
            NOTIFY_LOG_ERROR("seqNum %u is time out", seqNum);
        } else {
            ret = 0;
        }
    } while (0);
    pthread_mutex_unlock(&g_timerLock);
    return ret == (-1);
}

bool IsSeqEXistInTimerlist(unsigned int seqNum)
{
    int ret = -1;
    do {
        pthread_mutex_lock(&g_timerLock);
        if (g_timeListener->queueHead == NULL || g_timeListener->cnt == 0) {
            NOTIFY_LOG_ERROR("TimeoutList status error");
            break;
        }
        struct NotifyTimeOutNode *node = g_timeListener->queueHead;
        while (node != NULL && node->seqNum != seqNum) node = node->next;
        if (node != NULL) {
            ret = 0;
        }
    } while (0);
    pthread_mutex_unlock(&g_timerLock);
    return ret == (0);
}

int AddSeqNumInTimeoutList(unsigned int seqNum)
{
    pthread_mutex_lock(&g_timerLock);
    struct NotifyTimeOutNode *node = (struct NotifyTimeOutNode *)malloc(sizeof(struct NotifyTimeOutNode));
    if (node == NULL) {
        NOTIFY_LOG_ERROR("malloc Pool faild ret %d", (int)errno);
        pthread_mutex_unlock(&g_timerLock);
        return -1;
    }
    struct timeval t;
    gettimeofday(&t, NULL);
    node->next = NULL;
    node->seqNum = seqNum;
    node->tvSec = t.tv_sec + TIMEOUTSIZE;
    if (g_timeListener->queueHead == NULL) {
        g_timeListener->queueHead = node;
        /* 防止异常 */
        g_timeListener->cnt = 0;
    } else {
        struct NotifyTimeOutNode *temp = g_timeListener->queueHead;
        while (temp->next != NULL && node->tvSec > temp->tvSec) temp = temp->next;
        temp->next = node;
    }
    g_timeListener->cnt += 1;
    pthread_cond_signal(&g_timerWorkWaite);
    pthread_mutex_unlock(&g_timerLock);
    return 0;
}

void ReomveSeqNumFromTimeoutList(unsigned int seqNum)
{
    pthread_mutex_lock(&g_timerLock);
    if (g_timeListener->cnt == 0 || g_timeListener->queueHead == NULL) {
        /* 是否需要判断队列不为空则free,再置空 */
        g_timeListener->cnt = 0;
        g_timeListener->queueHead = NULL;
        NOTIFY_LOG_ERROR("TimeoutList status error");
        pthread_mutex_unlock(&g_timerLock);
        return;
    }
    struct NotifyTimeOutNode *node = g_timeListener->queueHead;
    if (node->seqNum == seqNum) {
        g_timeListener->queueHead = g_timeListener->queueHead->next;
    } else {
        while (node->next != NULL && node->next->seqNum != seqNum) {
            node = node->next;
        }
        if (node->next == NULL) {
            NOTIFY_LOG_ERROR("remove failed cant not find seqNum %u in time out list", seqNum);
            pthread_mutex_unlock(&g_timerLock);
            return;
        }
        struct NotifyTimeOutNode *temp = node->next;
        node->next = temp->next;
        free(temp);
    }
    g_timeListener->cnt -= 1;
    pthread_mutex_unlock(&g_timerLock);
    return;
}

static void NotifyClientTimeListenerWork(void)
{
    struct timeval t;
    while (1) {
        pthread_mutex_lock(&g_timerLock);
        if (g_timeListener->cnt == 0 || g_timeListener->queueHead == NULL) {
            pthread_mutex_unlock(&g_timerLock);
            return;
        }
        gettimeofday(&t, NULL);
        /* 有线程超时,唤醒所有阻塞线程，删除超时节点由sendnotify函数来调用接口处理 */
        if (t.tv_sec > g_timeListener->queueHead->tvSec) {
            DataLock();
            WakeupDataWaite();
            DataUnlock();
        }
        pthread_mutex_unlock(&g_timerLock);
        sleep(1); /* 无超时则休眠1秒 */
    }
}

static void *NotifyClientTimeListener(void *UNUSED(arg))
{
    while (g_timeListenerRun == 1) {
        pthread_mutex_lock(&g_timerLock);
        while (g_timeListener->cnt == 0) {
            pthread_cond_wait(&g_timerWorkWaite, &g_timerLock);
        }
        if (g_timeListenerRun != 1) {
            NOTIFY_LOG_WARN("timer listener pthread exit");
            pthread_mutex_unlock(&g_timerLock);
            break;
        }
        pthread_mutex_unlock(&g_timerLock);
        NotifyClientTimeListenerWork();
    }
    return NULL;
}

int CreateTimeOutListenerThread(void)
{
    pthread_t tid;
    if (g_timeListenerRun == 1) {
        NOTIFY_LOG_ERROR("timeout listener is exist");
        return -1;
    }
    g_timeListenerRun = 1;
    g_timeListener = (struct NotifyTimeOutList *)malloc(sizeof(struct NotifyTimeOutList));
    if (g_timeListener == NULL) {
        NOTIFY_LOG_ERROR("malloc faild ret %d", (int)errno);
        return -1;
    }
    if (pthread_create(&tid, NULL, (void *)NotifyClientTimeListener, NULL) != 0) {
        NOTIFY_LOG_ERROR("create thread faild ret %d", (int)errno);
        g_timeListenerRun = 0;
        return -1;
    }
    pthread_mutex_init(&g_timerLock, NULL);
    pthread_cond_init(&g_timerWorkWaite, NULL);
    return 0;
}

void DestoryTimeOutListenerThread(void)
{
    if (g_timeListenerRun == 0 || g_timeListener == NULL) {
        NOTIFY_LOG_ERROR("timeout listener is not run");
        return;
    }
    pthread_mutex_lock(&g_timerLock);
    struct NotifyTimeOutNode *head = g_timeListener->queueHead;
    while (head != NULL) {
        struct NotifyTimeOutNode *node = head;
        free(node);
        head = head->next;
    }
    g_timeListener->cnt = 0;
    g_timeListenerRun = 0;
    pthread_cond_signal(&g_timerWorkWaite);
    pthread_mutex_unlock(&g_timerLock);
    sleep(1); /* 等待线程退出之后再释放资源 */
    free(g_timeListener);
    g_timeListener = NULL;
    pthread_cond_destroy(&g_timerWorkWaite);
    pthread_mutex_destroy(&g_timerLock);
}