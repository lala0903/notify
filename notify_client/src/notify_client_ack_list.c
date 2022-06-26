
#include "notify_client_ack_list.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "notify_client_common.h"

/*
 * 创建链表保存server发送过来的同步消息返回值
 * sennotify函数通过seqNum从链表获取目标模块的返回值
 */

struct NotifyAckList {
    struct NotifyAckNode *head;
    unsigned int cnt;
};

static pthread_mutex_t g_ackListLock;
static struct NotifyAckList *g_ackList = NULL;

int NotifyAckLisitInit(void)
{
    g_ackList = (struct NotifyAckList *)malloc(sizeof(struct NotifyAckNode));
    if (g_ackList == NULL) {
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        return -1;
    }
    g_ackList->head = NULL;
    g_ackList->cnt = 0;
    pthread_mutex_init(&g_ackListLock, NULL);
    return 0;
}

void NotifyAckLisitDestroy(void)
{
    if (g_ackList == NULL) {
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        return;
    }

    pthread_mutex_lock(&g_ackListLock);
    while (g_ackList->head != NULL) {
        struct NotifyAckNode *temp = g_ackList->head;
        g_ackList->head = g_ackList->head->next;
        free(temp);
    }
    g_ackList->cnt = 0;
    free(g_ackList);
    g_ackList = NULL;
    pthread_mutex_unlock(&g_ackListLock);
    pthread_mutex_destroy(&g_ackListLock);
    return;
}

struct NotifyAckNode *CreateAckNote(unsigned int seqNum, void *buff, unsigned int len, int retValue)
{
    struct NotifyAckNode *node = (struct NotifyAckNode *)malloc(sizeof(struct NotifyAckNode));
    if (node == NULL) {
        return NULL;
    }
    if (len == 0 || buff == NULL) {
        node->buff = NULL;
        node->len = 0;
    } else {
        node->len = len;
        node->buff = malloc(len);
        if (node->buff == NULL) {
            free(node);
            return NULL;
        }
        memcpy(node->buff, buff, len);
    }
    node->next = NULL;
    node->seqNum = seqNum;
    node->retValue = retValue;
    return node;
}

int InsertNodeInAckList(struct NotifyAckNode *node)
{
    if (node == NULL) {
        NOTIFY_LOG_ERROR("node is NULL");
        return -1;
    }
    if (g_ackList == NULL) {
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        return -1;
    }
    NOTIFY_LOG_INFO("recive ack seq %d", node->seqNum);
    pthread_mutex_lock(&g_ackListLock);
    if (g_ackList->head == NULL) {
        g_ackList->head = node;
    } else {
        node->next = g_ackList->head;
        g_ackList->head = node;
    }
    g_ackList->cnt++;
    pthread_mutex_unlock(&g_ackListLock);
    return 0;
}

int IsSeqNumExistInAckList(unsigned int seqNum)
{
    pthread_mutex_lock(&g_ackListLock);
    if (g_ackList == NULL) {
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        return -1;
    }
    struct NotifyAckNode *temp = g_ackList->head;
    while (temp != NULL) {
        if (temp->seqNum == seqNum) {
            break;
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&g_ackListLock);
    if (temp == NULL) {
        return -1;
    }
    return 0;
}

void RemoveNodeFromAckList(unsigned int seqNum)
{
    if (g_ackList == NULL) {
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        return;
    }
    pthread_mutex_lock(&g_ackListLock);
    struct NotifyAckNode *node = g_ackList->head;
    if (node == NULL) {
        return;
    }
    if (node->seqNum == seqNum) {
        g_ackList->head = g_ackList->head->next;
        g_ackList->cnt--;
        pthread_mutex_unlock(&g_ackListLock);
        free(node->buff);
        free(node);
    }
    while (node->next != NULL) {
        if (node->next->seqNum != seqNum) {
            node = node->next;
            continue;
        }
        struct NotifyAckNode *temp = node->next;
        node->next = node->next->next;
        free(temp->buff);
        free(temp);
        g_ackList->cnt--;
        pthread_mutex_unlock(&g_ackListLock);
        break;
    }
    return;
}

void GetDataFromeAckList(unsigned int seqNum, void *buff, unsigned int len, int *retValue)
{
    if (g_ackList == NULL) {
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        return;
    }
    pthread_mutex_lock(&g_ackListLock);
    struct NotifyAckNode *temp = g_ackList->head;
    while (temp != NULL) {
        if (temp->seqNum == seqNum) {
            if (len != temp->len) {
                pthread_mutex_unlock(&g_ackListLock);
                return;
            }
            *retValue = temp->retValue;
            if (len != 0 && buff != NULL) {
                memcpy(buff, temp->buff, len);
            }
            break;
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&g_ackListLock);
    return;
}