#include "notify_server_send.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "notify_server_common.h"
#include "notify_server_recv.h"

struct MsgNode {
    struct MsgHeadInfo *head;
    int fd; /* 发送消息的文件描述符 */
    struct MsgNode *next;
};

struct MsgList {
    pthread_mutex_t queueLock;
    pthread_cond_t wait;
    struct MsgNode *queueHead;
    int cnt;
};

struct MsgList *g_MsgList = NULL;
static int g_MoudleIdSocketFd[MODULE_ID_MAX] = {0}; /* moduleid 对应的 socket fd */
static pthread_mutex_t g_sendLock = PTHREAD_MUTEX_INITIALIZER;

int AddMessageInList(struct MsgHeadInfo *head, int fd)
{
    if (head == NULL) {
        NOTIFY_LOG_ERROR("input is invalid");
        return -1;
    }
    pthread_mutex_lock(&g_MsgList->queueLock);
    if (g_MsgList->cnt > 1000) {
        NOTIFY_LOG_ERROR("had too many msg wait to process cant not push new");
        pthread_mutex_unlock(&g_MsgList->queueLock);
        return -1;
    }
    struct MsgNode *node = (struct MsgNode *)malloc(sizeof(struct MsgNode));
    if (node == NULL) {
        NOTIFY_LOG_ERROR("malloc failed ret %d", (int)errno);
        pthread_mutex_unlock(&g_MsgList->queueLock);
        return -1;   
    }
    node->next = NULL;
    node->head = head;
    node->fd = fd;
    if (g_MsgList->cnt != 0) {
        struct MsgNode *temp = g_MsgList->queueHead;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = node;
    } else {
        g_MsgList->queueHead = node;
    }
    g_MsgList->cnt += 1;
    pthread_mutex_unlock(&g_MsgList->queueLock);
    pthread_cond_signal(&g_MsgList->wait);
    return 0;
}

static int SendMsgToClient(struct MsgHeadInfo *head, int fd)
{
    char *buff = (char *)head;
    unsigned int dataLen = head->totalLen + sizeof(struct MsgHeadInfo);
    unsigned int sendLen = 0;
    pthread_mutex_lock(&g_sendLock);
    // NOTIFY_LOG_INFO("SendMsgToClient dest id %d fd[%d]", head->destId, fd);
    while (dataLen > sendLen) {
        int retLen = send(fd, buff + sendLen, dataLen - sendLen, 0);
        if (retLen > 0) {
            sendLen += retLen;
        } else {
            pthread_mutex_unlock(&g_sendLock);
            NOTIFY_LOG_ERROR("send data to client dest id %d fd[%d] failed %d",
                            head->destId, fd, (int)errno);
            return -1;
        }
    }
    pthread_mutex_unlock(&g_sendLock);
    return 0; 
}

static int RegisterMsgHandle(struct MsgHeadInfo *head, int fd)
{
    if (head->event == NOTIFY_REGISTER) {
        NOTIFY_LOG_INFO("register module [%d], fd %d", head->sourceId, fd);
        g_MoudleIdSocketFd[head->sourceId] = fd;
    } else if (head->event == NOTIFY_UNREGISTER) {
        NOTIFY_LOG_INFO("unregister module [%d], fd %d", head->sourceId, fd);
        RemoveClientListen(g_MoudleIdSocketFd[head->sourceId]);
        g_MoudleIdSocketFd[head->sourceId] = -1;
    } else if (head->event == SERVER_WRITE_SEND_TEST) {
        NOTIFY_LOG_INFO("SERVER_WRITE_SEND_TEST fd %d", g_MoudleIdSocketFd[head->sourceId]);
        head->msgType = ACK_MSG;
        head->ackType = ACK_OK;
        SendMsgToClient(head, fd);
    }
    return 0;
}

static int ForwardMsgToClient(struct MsgHeadInfo *head)
{
    int fd = g_MoudleIdSocketFd[head->destId];
    if (fd <= 0) {
        NOTIFY_LOG_ERROR("destination client socket fd[%d] invaild", fd);
        if (head->msgType == SNYC_MSG) {
            head->msgType = ASNYC_MSG;
            head->ackType = ACK_ERR;
            head->ackType = ACK_ERR;
            SendMsgToClient(head, g_MoudleIdSocketFd[head->sourceId]);
        }
        return -1;
    }
    return SendMsgToClient(head, fd);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           
}

static int PrintHeadInfo(struct MsgHeadInfo *head, char *buff)
{
    if (head == NULL) {
        NOTIFY_LOG_ERROR("head is NULL");
        return -1;
    }
    NOTIFY_LOG_INFO("%s seqNum %u sourceId %u msgType %u destId %u event %u syncType %u ackType %d totalLen %u par1Len %u par2Len %u outLen %u",
                    buff, head->seqNum, head->sourceId, head->msgType, head->destId,
                    head->event, head->syncType, (int)head->ackType, head->totalLen,
                    head->par1Len, head->par2Len, head->outLen);
    return 0;
}

static void ProcessClientMessage(struct MsgHeadInfo *head, int fd)
{
    PrintHeadInfo(head, "recive message ");
    if (head == NULL) {
        return;
    }
    if (head->destId == MODULE_NOTIFY_SERVER) { /* 发送给server的消息 */
        RegisterMsgHandle(head, fd);
    } else if (head->destId == MODULE_BROADCAST) { /* 广播 */
        
    } else { /* 发送给其他模组消息 */
        ForwardMsgToClient(head);
    }
    
    free(head);
}

static void *ServerMsgHandle(void *arg)
{
    while (1) {
        pthread_mutex_lock(&g_MsgList->queueLock);
        while (g_MsgList->cnt == 0) {
            pthread_cond_wait(&g_MsgList->wait, &g_MsgList->queueLock);
        }  

        if (g_MsgList->cnt == 0 || g_MsgList->queueHead == NULL) {
            NOTIFY_LOG_ERROR("g_MsgList status error cnt %d", g_MsgList->cnt);
            pthread_mutex_unlock(&g_MsgList->queueLock);
            break;
        }
        struct MsgNode *node = g_MsgList->queueHead;
        g_MsgList->cnt -= 1;
        g_MsgList->queueHead = g_MsgList->queueHead->next;
        pthread_mutex_unlock(&g_MsgList->queueLock);
        ProcessClientMessage(node->head, node->fd);
        free(node);
        node = NULL;
    }
    return NULL;
}

int InitMessageList(void)
{
    g_MsgList = (struct MsgList *)malloc(sizeof(struct MsgList));
    if (g_MsgList == NULL) {
        NOTIFY_LOG_ERROR("malloc list faild ret %d", (int)errno);
        return -1;
    }
    memset((void *)g_MsgList, 0, sizeof(struct MsgList));
    pthread_mutex_init(&g_MsgList->queueLock, NULL);
    pthread_cond_init(&g_MsgList->wait, NULL);

    pthread_t tid;
    if (pthread_create(&tid, NULL, (void *)ServerMsgHandle, NULL) != 0) {
        NOTIFY_LOG_ERROR("create thread faild ret %d", (int)errno);
        return -1;
    }
    return 0;
}