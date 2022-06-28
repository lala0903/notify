
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
    if (g_ackList != NULL) {
        NOTIFY_LOG_ERROR("ack list is initialized");
        return -1;
    }
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
        NOTIFY_LOG_ERROR("ack list is realsed");
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
}

struct NotifyAckNode *CreateAckNote(struct MsgHeadInfo *head)
{
    struct NotifyAckNode *node = (struct NotifyAckNode *)malloc(sizeof(struct NotifyAckNode));
    if (node == NULL) {
        return NULL;
    }
    node->next = NULL;
    node->seqNum = head->seqNum;
    node->head = head;
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

bool IsSeqNumExistInAckList(unsigned int seqNum)
{
    pthread_mutex_lock(&g_ackListLock);
    if (g_ackList == NULL) {
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        pthread_mutex_unlock(&g_ackListLock);
        return false;
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
        return false;
    }
    return true;
}

struct NotifyAckNode *RemoveNodeFromAckList(unsigned int seqNum)
{
    pthread_mutex_lock(&g_ackListLock);
    if (g_ackList == NULL) {
        pthread_mutex_unlock(&g_ackListLock);
        NOTIFY_LOG_ERROR("g_ackList is NULL");
        return NULL;
    }
    struct NotifyAckNode *node = g_ackList->head;
    if (node == NULL) {
        pthread_mutex_unlock(&g_ackListLock);
        return NULL;
    }
    if (node->seqNum == seqNum) {
        g_ackList->head = g_ackList->head->next;
        g_ackList->cnt--;
        pthread_mutex_unlock(&g_ackListLock);
        return node;
    }
    while (node->next != NULL) {
        if (node->next->seqNum != seqNum) {
            node = node->next;
            continue;
        }
        struct NotifyAckNode *temp = node->next;
        node->next = node->next->next;
        g_ackList->cnt--;
        pthread_mutex_unlock(&g_ackListLock);
        return temp;
    }
    pthread_mutex_unlock(&g_ackListLock);
    return NULL;
}

void ReleaseAckNode(struct NotifyAckNode *node)
{
    if (node == NULL) {
        NOTIFY_LOG_ERROR("node is NULL");
        return;
    }
    if (node->head != NULL) {
        free(node->head);
    }
    free(node);
}