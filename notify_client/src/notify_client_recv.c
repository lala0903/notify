#include "notify_client_recv.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include "threadpool.h"
#include "notify_client_list.h"
#include "notify_client_ack_list.h"
#include "notify_client_send.h"
#include "notify_client_common.h"

#define ONCE_READ_SIZE 1024

static int g_recvThreadRun = 0;

static int RecvMsgFromServer(void *buff, size_t len)
{
    size_t totalLen = 0;
    while (totalLen < len) {
        size_t readLen = len - totalLen >= ONCE_READ_SIZE ? ONCE_READ_SIZE : len - totalLen;
        ssize_t ret = recv(GetClientSocket(), buff + totalLen, readLen, 0);
        if (ret < 0) {
            NOTIFY_LOG_ERROR("read message faild ret %d", (int)errno);
            return -1;
        } else if (ret == 0) {
            
        } else {
            totalLen += ret;
        }
    }
    return totalLen;
}

static int RecvMsgHead(struct MsgHeadInfo *head)
{
    int retLen = RecvMsgFromServer(head, sizeof(struct MsgHeadInfo));
    if (retLen != sizeof(struct MsgHeadInfo)) {
        NOTIFY_LOG_ERROR("read message head failed");
        return -1;
    }
    return 0;
}

static int RecvMsgBody(char *buff, int len)
{
    int retLen = RecvMsgFromServer(buff + sizeof(struct MsgHeadInfo), len);
    if (retLen != len) {
        NOTIFY_LOG_ERROR("read message body failed");
        return -1;
    }
    return 0;
}

static struct MsgHeadInfo *RecvMsg(void)
{
    struct MsgHeadInfo head;
    memset((void *)&head, 0, sizeof(struct MsgHeadInfo));
    if (RecvMsgHead(&head) != 0) {
        NOTIFY_LOG_ERROR("read msg head failed");
        return NULL;
    }
    int bodyLen = head.totalLen;
    if (bodyLen < 0 || bodyLen > INT_MAX + sizeof(struct MsgHeadInfo) + 1) {
        NOTIFY_LOG_ERROR("bodyLen is invalid %d", bodyLen);
        return -1;
    }
    char *buff = (char *)malloc(bodyLen + sizeof(struct MsgHeadInfo));
    if (buff == NULL) {
        NOTIFY_LOG_ERROR("malloc faild ret %d", (int)errno);
        return -1;
    }
    memcpy((void *)buff, (void *)&head, sizeof(struct MsgHeadInfo));
    if (head.totalLen == 0) {
        return (struct MsgHeadInfo *)buff;
    }
    if (RecvMsgBody(buff, (size_t)bodyLen) != 0) {
        free(buff);
        NOTIFY_LOG_ERROR("padding msg body failed");
        return NULL;
    }
    return (struct MsgHeadInfo *)buff;
}

static void *AsyncMsgHandle(void *arg)
{
    if (arg == NULL) return NULL;
    struct MsgHeadInfo *head = (struct MsgHeadInfo *)arg;
    do {
        if (IS_MODULEID_INVAILD(head->destId)) {
            NOTIFY_LOG_ERROR("module id %d is invalid", head->destId);
            break;
        }
        if ((head->par1Len > head->totalLen) || (head->par2Len > head->totalLen) ||
            (head->par1Len + head->par2Len != head->totalLen)) {
            NOTIFY_LOG_ERROR("par len is invalid par1Len %u par2Len %u totalLen %u",
                            head->par1Len, head->par2Len, head->totalLen);
            break;
        }
        RegisterAsyncFunc proc = GetAsyncFunc(head->destId);
        if (proc != NULL && head->ackType == NO_ACK) {
            char *buff1 = head->par1Len == 0 ? NULL : head + 1;
            char *buff2 = head->par2Len == 0 ? NULL : (void *)(head + 1) + head->par1Len;
            (void)proc(head->event, buff1, head->par1Len, buff2, head->par2Len);
        } else if (head->ackType == ACK_ERR) {
            /* 如果服务器未找到目标模块，则需要通知退出阻塞的同步发送业务 */
            DataLock();
            ClearDestModuleIdValue(head->sourceId);
            WakeupDataWaite();
            DataUnlock();
        }
    } while (0);
    free(head);
    return NULL;
}

static void *SyncMsgHandle(void *arg)
{
    int retValue = -1;
    char *buff1 = NULL;
    char *buff2 = NULL;
    unsigned int inLen = 0;
    unsigned int outLen = 0;
    struct MsgHeadInfo *head = (struct MsgHeadInfo *)arg;
    do {
        if (IS_MODULEID_INVAILD(head->destId)) {
            NOTIFY_LOG_ERROR("module id %d is invalid", head->destId);
            break;
        }
        if ((head->par1Len > head->totalLen) || (head->outLen > head->totalLen) ||
            (head->par1Len + head->outLen != head->totalLen)) {
            NOTIFY_LOG_ERROR("par len is invalid par1Len %u outLen %u totalLen %u",
                            head->par1Len, head->outLen, head->totalLen);
            break;
        }
        RegisterSyncFunc proc = GetSyncFunc(head->destId);
        if (proc == NULL) {
            NOTIFY_LOG_ERROR("module id %d is not register", head->destId);
            break;
        }
        inLen = head->par1Len;
        outLen = head->outLen;
        buff1 = inLen == 0 ? NULL : head + 1;
        buff2 = outLen == 0 ? NULL : (void *)(head + 1) + head->par1Len;
        retValue = proc(head->event, buff1, head->par1Len, buff2, head->outLen);
    } while (0);
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, ACK_MSG, head->sourceId, 0, buff1, inLen, buff2, outLen, retValue);
    (void)SendMsgToServer(&msg, head->seqNum);
    free(head);
    return NULL;
}

static void *AckMsgHandle(void *arg)
{
    struct MsgHeadInfo *head = (struct MsgHeadInfo *)arg;
    void *buff = head + 1;
    struct NotifyAckNode *node = CreateAckNote(head->seqNum, buff, head->outLen,
                                               head->retValue);
    InsertNodeInAckList(node);
    WakeupDataWaite();
    free(head);
    return NULL;
}

static void *NotifyClientRecv(void *UNUSED(arg))
{
    fd_set rfds;
    while (g_recvThreadRun == 1) {
        if (GetClientSocket() < 0) break;
        FD_ZERO(&rfds);
        FD_SET(GetClientSocket(), &rfds);
        int ret = select(GetClientSocket() + 1, &rfds, NULL, NULL, NULL);
        if (ret > 0) {
            struct MsgHeadInfo *head = RecvMsg();
            if (head == NULL) {
                NOTIFY_LOG_ERROR("read message failed");
                break;
            }
            if (head->syncType == ASYNC_TYPE) {
                if (IsThreadPoolInit()) {
                    (void)AddTaskInThreadPool(AsyncMsgHandle, head);
                } else {
                    (void)AsyncMsgHandle(head);
                }
                continue;
            }
            if (head->ackType == ACK_MSG) {
                AckMsgHandle(head);
                continue;
            }
            if (IsThreadPoolInit()) {
                (void)AddTaskInThreadPool(SyncMsgHandle, head);
            } else {
                (void)SyncMsgHandle(head);
            }
        } else {
            NOTIFY_LOG_ERROR("select failed ret = %d", (int)errno);
            break;
        }
    }
    return NULL;
}

int CreateRecvThread(void)
{
    pthread_t tid;
    g_recvThreadRun = 1;
    if (pthread_create(&tid, NULL, (void *)NotifyClientRecv, NULL) != 0) {
        NOTIFY_LOG_ERROR("create thread faild ret %d", (int)errno);
        g_recvThreadRun = 0;
        return -1;
    }
    return 0;
}

void DestoryRecvThread(void)
{
    g_recvThreadRun = 0;
}