#include "notify_client_recv.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include "threadpool.h"
#include "notify_client_ack_list.h"
#include "notify_client_send.h"
#include "notify_client_init.h"
#include "notify_client_time.h"
#include "notify_client_common.h"

/*
 * 监控socket是否可读，如果可读则先读出协议头，再根据数据长度读出数据
 * 根据协议头的数据处理对应的异步、同步、应答消息
 * 异步：根据module id回调对应的函数指针
 * 同步：根据module id回调对应的函数指针，并将结果组包发送一个ack
 * 应答：将服务器发送过来的数据插入ack list,并唤醒所有阻塞的sendnotify线程
 */

#define ONCE_READ_SIZE 1024
#define RETRY_ADDPOOL_TIME_MAX 20

static int g_recvThreadRun = 0;

/* 
 * read返回0，对方正常调用close关闭链接
 * read返回-1，需要通过errno来判断，如果不是EAGAIN和EINTR，那么就是对方异常断开链接
 * 两种情况服务端都要close套接字
 */
static int RecvMsgFromServer(void *buff, size_t len)
{
    size_t totalLen = 0;
    while (totalLen < len) {
        size_t readLen = len - totalLen >= ONCE_READ_SIZE ? ONCE_READ_SIZE : len - totalLen;
        ssize_t ret = recv(GetClientSocket(), buff + totalLen, readLen, 0);
        if (ret > 0) {
            totalLen += ret;
        } else if (ret == 0) {
            CloseClientSocket();
            NOTIFY_LOG_WARN("read message ret %d", (int)ret);
            return -1;
        } else {
            if (errno == EAGAIN || errno == EINTR) {
                NOTIFY_LOG_WARN("get signal EAGAIN || EINTR %d", (int)errno);
                continue;
            }
            CloseClientSocket();
            NOTIFY_LOG_ERROR("read message faild ret %d", (int)errno);
            return -1;
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
        return NULL;
    }
    char *buff = (char *)malloc(bodyLen + sizeof(struct MsgHeadInfo));
    if (buff == NULL) {
        NOTIFY_LOG_ERROR("malloc faild ret %d", (int)errno);
        return NULL;
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
    struct MsgHeadInfo *head = (struct MsgHeadInfo *)arg;
    do {
        if ((head->par1Len > head->totalLen) || (head->par2Len > head->totalLen) ||
            (head->par1Len + head->par2Len != head->totalLen)) {
            NOTIFY_LOG_ERROR("par len is invalid par1Len %u par2Len %u totalLen %u",
                             head->par1Len, head->par2Len, head->totalLen);
            break;
        }
        RegisterAsyncFunc proc = GetAsyncFunc(head->destId);
        if (proc != NULL && head->ackType == NO_ACK) {
            char *buff1 = (head->par1Len == 0) ? NULL : (void *)(head + 1);
            char *buff2 = (head->par2Len == 0) ? NULL : (void *)(head + 1) + head->par1Len;
            (void)proc(head->event, buff1, head->par1Len, buff2, head->par2Len);
        } else if (head->ackType == ACK_ERR) {
            /* 如果服务器未找到目标模块，则需要通知退出阻塞的同步发送业务 */
            NOTIFY_LOG_ERROR("recive server ack error msg, source module id %u", head->sourceId);
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
        buff1 = inLen == 0 ? NULL : (void *)(head + 1);
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
    if (head->totalLen != head->outLen) {
        NOTIFY_LOG_ERROR("mssage type[%d] or totalLen %u != outLen %u is invalid",
                         head->msgType, head->totalLen, head->outLen);
        goto EXIT;
    }
    /* 如果ack已经存在或超时监听找不到seq，则丢弃这一包ack */
    if (IsSeqNumExistInAckList(head->seqNum)) {
        NOTIFY_LOG_ERROR("head->seqNum %u is exist", head->seqNum);
        goto EXIT;
    }
    if (!IsSeqEXistInTimerlist(head->seqNum)) {
        NOTIFY_LOG_ERROR("head->seqNum %u is not exist in time list", head->seqNum);
        goto EXIT;
    }
    struct NotifyAckNode *node = CreateAckNote(head);
    if (node == NULL) {
        NOTIFY_LOG_ERROR("Create Client ack node faild");
        goto EXIT;
    }
    InsertNodeInAckList(node);
    DataLock();
    WakeupDataWaite();
    DataUnlock();
    return NULL;
EXIT:
    free(head);
    return NULL;
}

static int ReconnectServer(void)
{
    if (ConnectNotifyServer() < 0) {
        NOTIFY_LOG_ERROR("Create Client Socket failed");
        return -1;
    }
    if (RegisterAgain() < 0) {
        NOTIFY_LOG_ERROR("register again failed");
        return -1;
    }
    return 0;
}

static bool IsMsgInvalid(struct MsgHeadInfo *head)
{
    if (head->syncType != ASYNC_TYPE && head->syncType != SYNC_TYPE) {
        NOTIFY_LOG_ERROR("message sync type[%d] is invalid", head->syncType);
        return true;
    }
    /* 不允许出现通过服务器发送给自己的消息 */
    if (head->destId == head->sourceId) {
        NOTIFY_LOG_ERROR("module id %d is invalid", head->destId);
        return true;
    }
    if (IS_MODULEID_INVAILD(head->destId)) {
        NOTIFY_LOG_ERROR("module id %d is invalid", head->destId);
        return true;
    }
    return false;
}

static int ReciveMsgHandle(void)
{
    int i;
    struct MsgHeadInfo *head = RecvMsg();
    if (head == NULL) {
        NOTIFY_LOG_ERROR("read message failed");
        return -1;
    }
    PrintHeadInfo(head, "recive message ");
    if (IsMsgInvalid(head)) {
        free(head);
        return -1;
    }
    if (head->syncType == ASYNC_TYPE) {
        if (head->msgType != ASNYC_MSG) {
            NOTIFY_LOG_ERROR("message async type[%d] is invalid msgtype %d", head->syncType, head->msgType);
            free(head);
            return -1;
        }
        if (IsThreadPoolInit()) {
            (void)AddTaskInThreadPool(AsyncMsgHandle, head);
        } else {
            (void)AsyncMsgHandle(head);
        }
        return 0;
    }
    if (head->msgType == ACK_MSG) {
        AckMsgHandle(head);
        return 0;
    }
    if (head->msgType != SNYC_MSG) {
        NOTIFY_LOG_ERROR("message sync type[%d] is invalid msgtype %d", head->syncType, head->msgType);
        free(head);
        return -1;
    }
    if (IsThreadPoolInit()) {
        for (i = 0; i < RETRY_ADDPOOL_TIME_MAX; i++) {
            if (AddTaskInThreadPool(SyncMsgHandle, head) == 0) break;
        }
         /* 加入线程池出错时需要回应一个ack */
        if (i == RETRY_ADDPOOL_TIME_MAX) {
            ReplyMessage(head);
            return -1;
        }
    } else {
        (void)SyncMsgHandle(head);
    }
    return 0;
}

static void *NotifyClientRecv(void *UNUSED(arg))
{
    fd_set rfds;
    int retTryCnt = 0;
    while (g_recvThreadRun == 1) {
        if (GetClientSocket() < 0) {
            NOTIFY_LOG_WARN("disconnect from server and try to reconnect time %d", retTryCnt);
            if (ReconnectServer() < 0) {
                retTryCnt++;
                sleep(1); /* 休眠一秒再从重连服务器 */
                continue;
            }
            retTryCnt = 0;
        }
        FD_ZERO(&rfds);
        FD_SET(GetClientSocket(), &rfds);
        int ret = select(GetClientSocket() + 1, &rfds, NULL, NULL, NULL);
        if (g_recvThreadRun == 0) {
            NOTIFY_LOG_WARN("recive thread eixt");
            break;
        }
        if (ret > 0) {
            ReciveMsgHandle();
        } else {
            NOTIFY_LOG_ERROR("select failed ret = %d", (int)errno);
        }
    }
    return NULL;
}

int CreateRecvThread(void)
{
    if (g_recvThreadRun == 1) {
        NOTIFY_LOG_WARN("recive thread is exist");
        return -1;
    }
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