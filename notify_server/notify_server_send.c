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
#include <sys/epoll.h>
#include "notify_server_common.h"
#include "notify_server_recv.h"
#include "notify_server_init.h"

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
static int g_MoudleIdSocketFd[MODULE_ID_MAX] = { 0 }; /* moduleid 对应的 socket fd */
static pthread_mutex_t g_sendLock = PTHREAD_MUTEX_INITIALIZER;

void ClearRegisteredClientSocekt(int clientFd)
{
    if (clientFd <= 0) {
        NOTIFY_LOG_ERROR("input is invalid");
        return;
    }
    for (size_t i = 0; i < sizeof(g_MoudleIdSocketFd) / sizeof(g_MoudleIdSocketFd[0]); i++) {
        if (g_MoudleIdSocketFd[i] != clientFd) continue;
        g_MoudleIdSocketFd[i] = -1;
        NOTIFY_LOG_INFO("clear module [%d] register", (int)i);
        break;
    }
}

int AddMessageInList(struct MsgHeadInfo *head, int fd)
{
    if (head == NULL) {
        NOTIFY_LOG_ERROR("input is invalid");
        return -1;
    }
    if (g_MsgList == NULL) {
        NOTIFY_LOG_ERROR("message is not ready");
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

/*
 * send返回0，对方正常调用close关闭链接
 * send返回-1，需要通过errno来判断，如果不是EAGAIN和EINTR，那么就是对方异常断开链接
 * 两种情况服务端都要close套接字,并清除注册
*/
static int SendMsgToClient(struct MsgHeadInfo *head, int fd)
{
    int ret = 0;
    char *buff = (char *)head;
    unsigned int dataLen = head->totalLen + sizeof(struct MsgHeadInfo);
    unsigned int sendLen = 0;
    pthread_mutex_lock(&g_sendLock);
    while (dataLen > sendLen) {
        int retLen = send(fd, buff + sendLen, dataLen - sendLen, 0);
        if (retLen > 0) {
            sendLen += retLen;
        } else if (retLen == 0) {
            RemoveClientListen(fd);
            ret = -1;
            break;
        } else {
            if (errno == EAGAIN || errno == EINTR) {
                NOTIFY_LOG_WARN("get signal EAGAIN || EINTR %d", (int)errno);
                continue;
            }
            RemoveClientListen(fd);
            NOTIFY_LOG_ERROR("send data to client dest id %d fd[%d] failed %d",
                             head->destId, fd, (int)errno);
            ret = -1;
            break;
        }
    }
    pthread_mutex_unlock(&g_sendLock);
    return 0; 
}

static int RegisterMsgHandle(struct MsgHeadInfo *head, int fd)
{
    if (head->event == SERVER_WRITE_SEND_TEST) {
        /* 客户端读写测试，如果是同步消息则将客户端发送过来的消息发送回给客户端 */
        NOTIFY_LOG_INFO("SERVER_WRITE_SEND_TEST module %u fd %d", head->sourceId, g_MoudleIdSocketFd[head->sourceId]);
        if (head->msgType == SNYC_MSG) {
            head->msgType = ACK_MSG;
            head->ackType = ACK_OK;
            head->destId = head->sourceId;
            head->sourceId = MODULE_NOTIFY_SERVER;
            /* 注客户端 sendnotify 必须 inlen <= outlen */
            if (head->outLen != 0 && head->outLen <= head->par1Len) {
                head->totalLen = head->outLen;
            } else {
                head->totalLen = 0;
                head->outLen = 0;
            }
            head->par1Len = 0;
            head->par2Len = 0;
            return SendMsgToClient(head, fd);
        }
        return 0;
    }
    int moduleId;
    if (head->par1Len == sizeof(int)) {
        moduleId = *(int *)(head + 1);
    } else {
        moduleId = head->sourceId;
        NOTIFY_LOG_WARN("par1Len [%d] is invalid", head->par1Len);
    }
    if (head->event == NOTIFY_REGISTER) {
        NOTIFY_LOG_INFO("source id [%d] register module [%d], fd %d", head->sourceId, moduleId, fd);
        g_MoudleIdSocketFd[moduleId] = fd;
    } else if (head->event == NOTIFY_UNREGISTER) {
        NOTIFY_LOG_INFO("source id [%d] unregister module [%d], fd %d", head->sourceId, moduleId, fd);
        RemoveClientListen(g_MoudleIdSocketFd[moduleId]);
        g_MoudleIdSocketFd[moduleId] = -1;
        // for (int i = 0; i < MODULE_ID_MAX; i++) {
        //     if (fd == g_MoudleIdSocketFd[i]) return 0;
        // }
    }
    return 0;
}

static int PrintHeadInfo(struct MsgHeadInfo *head, char *buff)
{
    NOTIFY_LOG_INFO("%s sourceId %u destId %u event %u seqNum %u msgType %u syncType %u ackType %d totalLen %u par1Len %u par2Len %u outLen %u",
                    buff, head->sourceId, head->destId, head->event, head->seqNum,
                    head->msgType, head->syncType, (int)head->ackType, head->totalLen,
                    head->par1Len, head->par2Len, head->outLen);
    return 0;
}

static int ForwardMsgToClient(struct MsgHeadInfo *head)
{
    int fd = g_MoudleIdSocketFd[head->destId];
    if (fd > 0) {
        return SendMsgToClient(head, fd);
    }
    NOTIFY_LOG_ERROR("dest module[%d] client socket invaild", head->destId);
    if (head->msgType == SNYC_MSG) {
        struct MsgHeadInfo temp;
        memset((void *)&temp, 0, sizeof(temp));
        temp.msgType = ASNYC_MSG;
        temp.syncType = ASYNC_TYPE;
        temp.ackType = ACK_ERR;
        temp.destId = head->sourceId;
        temp.sourceId = head->destId;
        temp.seqNum = head->seqNum;
        NOTIFY_LOG_ERROR("SNYC_MSG send error ack to client module[%u], fd[%d]",
                         temp.destId, g_MoudleIdSocketFd[temp.destId]);
        PrintHeadInfo(&temp, "send message");
        SendMsgToClient(&temp, g_MoudleIdSocketFd[temp.destId]);
    } else {
        NOTIFY_LOG_ERROR("ASNYC_MSG ignore");
    }
    return -1;
}

static void ProcessClientMessage(struct MsgHeadInfo *head, int fd)
{
    if (head == NULL || fd < 0) {
        NOTIFY_LOG_ERROR("input is invalid head %p fd %d", head, fd);
        return;
    }
    PrintHeadInfo(head, "recive message ");
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
    if (g_MsgList != NULL) {
        NOTIFY_LOG_WARN("message list is initialized");
        return -1;
    }
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
        free(g_MsgList);
        g_MsgList == NULL;
        return -1;
    }
    return 0;
}

void DestoryMessageList(void)
{
    if (g_MsgList == NULL) {
        NOTIFY_LOG_WARN("message list is uninitialized");
        return;
    }
    pthread_mutex_lock(&g_MsgList->queueLock);
    struct MsgNode *node = g_MsgList->queueHead;
    while (node != NULL) {
        struct MsgNode *temp = node;
        node = node->next;
        if (temp->head != NULL) {
            free(temp->head);
        }
        free(temp);
    }
    g_MsgList->cnt = 0;
    pthread_cond_signal(&g_MsgList->wait);
    pthread_mutex_unlock(&g_MsgList->queueLock);
    for (int i = 0; i < MODULE_ID_MAX; i++) {
        if (g_MoudleIdSocketFd[i] > 0) {
            if (epoll_ctl(GetEpollFd(), EPOLL_CTL_DEL, g_MoudleIdSocketFd[i], NULL) < 0) {
                NOTIFY_LOG_ERROR("Remove client fd failed ret %d", (int)errno);
            }
            close(g_MoudleIdSocketFd[i]);
            g_MoudleIdSocketFd[i] = -1;
        }
    }
    free(g_MsgList);
    g_MsgList = NULL;
}