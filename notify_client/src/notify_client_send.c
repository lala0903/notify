#include "notify_client_send.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include "threadpool.h"
#include "notify_client_init.h"
#include "notify_client_common.h"
#include "notify_client_recv.h"
#include "notify_client_ack_list.h"

struct AsyncMsgHead {
    unsigned int destId;
    unsigned int event;
    void *par1;
    unsigned int par1Len;
    void *par2;
    unsigned int par2Len;
};

static long long g_DestMoudleId[MODULE_ID_MAX]; /* 保存对目标模块发送的序号.long long 防止溢出 */
static int g_currentMoudleId = 0; /* 同一进程使用第一个注册的moduleid 与server通信 */
static RegisterAsyncFunc g_asyncFunc[MODULE_ID_MAX] = {NULL};
static RegisterSyncFunc g_syncFunc[MODULE_ID_MAX] = {NULL};
static unsigned int g_sequenceNum = 0; //同步消息用于接收数据，异步无用

static pthread_mutex_t g_seqNumLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_sendLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_dateLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_dateWait = PTHREAD_COND_INITIALIZER;

void ClearDestModuleIdValue(NotifyModuleId moduleId)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return;
    }
    g_DestMoudleId[moduleId] = -1;
}

void WakeupDataWaite(void)
{
    pthread_cond_broadcast(&g_dateWait);
}

void DataLock(void)
{
    pthread_mutex_lock(&g_dateLock);
}

void DataUnlock(void)
{
    pthread_mutex_unlock(&g_dateLock);
}

unsigned int GetSequenceNum(void)
{
    pthread_mutex_lock(&g_seqNumLock);
    unsigned int seqNum = g_sequenceNum;
    pthread_mutex_unlock(&g_seqNumLock);
    return seqNum;
}

static int RegisterToServer(NotifyModuleId moduleId)
{
    if  (GetClientSocket() < 0) {
        NOTIFY_LOG_ERROR("socket is not ready");
        return -1;
    }
    if (g_currentMoudleId == 0) {
        g_currentMoudleId = moduleId;
    }
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, REG_MSG, MODULE_NOTIFY_SERVER, NOTIFY_REGISTER,
                        (void*)&g_currentMoudleId, sizeof(g_currentMoudleId), NULL, 0, 0);
    /* 异步 sequence number 紧要 */
    return SendMsgToServer(&msg, g_sequenceNum);
}

int RegisterNotifyFunction(NotifyModuleId moduleId, RegisterAsyncFunc asyncFunc, RegisterSyncFunc syncFunc)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return -1;
    }
    NOTIFY_LOG_INFO("register module %d", moduleId);
    g_asyncFunc[moduleId] = asyncFunc;
    g_syncFunc[moduleId] = syncFunc;
    if (RegisterToServer(moduleId) == 0) {
        return 0;
    }
    NOTIFY_LOG_ERROR("register module to server failed");
    return -1;
}

RegisterAsyncFunc GetAsyncFunc(NotifyModuleId moduleId)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return NULL;
    }
    return g_asyncFunc[moduleId];
}

RegisterSyncFunc GetSyncFunc(NotifyModuleId moduleId)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return NULL;
    }
    return g_syncFunc[moduleId];
}

void UnregisterNotifyFunction(void)
{
    /* 发送消息给server注销id */
    g_currentMoudleId = 0;
}

static int SendSyncMsg(const struct SendMsgFrame *msg)
{
    pthread_mutex_lock(&g_seqNumLock);
    unsigned int seqNum = g_sequenceNum++;
    pthread_mutex_unlock(&g_seqNumLock);
    if (IsSeqNumExistInAckList(seqNum) == 0) {
        RemoveNodeFromAckList(seqNum);
        NOTIFY_LOG_ERROR("seqNum %u is exist in ack list");
        return -1;
    }
    if (SendMsgToServer(msg, seqNum) < 0) {
        NOTIFY_LOG_ERROR("seqNum %u send message to server failed", seqNum);
        return -1;
    }
    if (AddSeqNumInTimeoutList(seqNum) < 0) {
        NOTIFY_LOG_ERROR("seqNum %u insert time out listener failed", seqNum);
        return -1;
    }
    pthread_mutex_lock(&g_dateLock);
    int retValue = -1;
    g_DestMoudleId[msg->destId] = seqNum;
    while (1) {
        /* 超时 */
        if (IsSeqNumTimeOut(seqNum) < 0) {
            break;
        }
        /* 如果没有超时，且存在ack mssage 则说明需要返回了处理结果了 */
        if (IsSeqNumExistInAckList(seqNum) == 0) {
            GetDataFromeAckList(seqNum, msg->par2, msg->par2Len, &retValue);
            RemoveNodeFromAckList(seqNum);
            break;
        }
        pthread_cond_wait(&g_dateWait, &g_dateLock);
        if (g_DestMoudleId[msg->destId] == -1) {
            NOTIFY_LOG_ERROR("can not recive ack from server");
            break;
        }
    }
    ReomveSeqNumFromTimeoutList(seqNum);
    pthread_mutex_unlock(&g_dateLock);
    return retValue;
}

int SendNotify(NotifyModuleId moduleId, NotifyEvent event, const void *input, unsigned int inLen,
               void *output, unsigned int outLen)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return -1;
    }
    /* 同进程回调 */
    RegisterSyncFunc proc = GetSyncFunc(moduleId);
    if (proc != NULL) {
        return proc(event, input, inLen, output, outLen);
    }
    if  (GetClientSocket() < 0) {
        NOTIFY_LOG_ERROR("socket is not ready");
        return -1;
    }
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, SNYC_MSG, moduleId, event, input, inLen, output, outLen, 0);
    return SendSyncMsg(&msg);
}

// 处理返回值
static unsigned int GetMsgBodyLen(const struct SendMsgFrame *msg, struct MsgHeadInfo *head)
{
    unsigned int bodyLen = 0;
    switch (msg->msgType) {
    case ASNYC_MSG:
        head->totalLen = msg->par1Len + msg->par2Len;
        head->par1Len = msg->par1Len;
        head->par2Len = msg->par2Len;
        break;
    case SNYC_MSG:
        head->totalLen = msg->par1Len + msg->par2Len;
        head->par1Len = msg->par1Len;
        head->outLen = msg->par2Len;
        break;
    case REG_MSG:
        head->totalLen = msg->par1Len;
        head->par1Len = msg->par1Len;
        break;
    case ACK_MSG:
        head->totalLen = msg->par2Len;
        head->outLen = msg->par2Len;
        break;
    default:
        break;
    }
    bodyLen = head->totalLen;
    return bodyLen;
}

static void PaddingMsgBody(void *buff, const struct SendMsgFrame *msg)
{
    switch (msg->msgType) {
    case ASNYC_MSG:
    case SNYC_MSG:
        if (msg->par1Len != 0 && msg->par1 != NULL) {
            memcpy(buff, msg->par1, msg->par1Len);
        } else if (msg->par1Len == 0 && msg->par1 == NULL) {

        } else {
            NOTIFY_LOG_ERROR("SendMsgFrame is invalid");
        }
        if (msg->par2Len != 0 && msg->par2 != NULL) {
            memcpy(buff + msg->par1Len, msg->par2, msg->par2Len);
        }
        break;
    case REG_MSG:
        if (msg->par1Len != 0 && msg->par1 != NULL) {
            memcpy(buff, msg->par1, msg->par1Len);
        }
        break;
    case ACK_MSG:
        if (msg->par2Len != 0 && msg->par2 != NULL) {
            memcpy(buff, msg->par2, msg->par2Len);
        }
        break;
    default:
        break;
    }
}

static void PaddingMsgHead(struct MsgHeadInfo *head, const struct SendMsgFrame *msg)
{
    head->sourceId = g_currentMoudleId;
    head->msgType = msg->msgType;
    head->destId = msg->destId;
    head->event = msg->event;
    head->retValue = msg->retValue;
    if (msg->msgType == ACK_MSG) {
        head->ackType = ACK_OK;
    } else {
        head->ackType = NO_ACK;
    }
    if (msg->msgType == ASNYC_MSG || msg->msgType == REG_MSG) {
        head->syncType = ASYNC_TYPE;
    } else {
        head->syncType = SYNC_TYPE;
    }
}

int PrintHeadInfo(struct MsgHeadInfo *head, char *buff)
{
    if (head == NULL) {
        NOTIFY_LOG_ERROR("head is NULL");
        return 0;
    }
    NOTIFY_LOG_INFO("%s seqNum %u sourceId %u msgType %u destId %u event %u syncType %u ackType %d totalLen %u par1Len %u par2Len %u outLen %u retValue %d",
                    buff, head->seqNum, head->sourceId, head->msgType, head->destId,
                    head->event, head->syncType, (int)head->ackType, head->totalLen,
                    head->par1Len, head->par2Len, head->outLen, head->retValue);
    return 0;
}

int SendMsgToServer(const struct SendMsgFrame *msg, unsigned int seqNum)
{
    struct MsgHeadInfo head;
    memset((void *)&head, 0, sizeof(head));
    PaddingMsgHead(&head, msg);
    head.seqNum = seqNum;
    unsigned int bodyLen = GetMsgBodyLen(msg, &head);
    char *buff = (char *)malloc(sizeof(struct MsgHeadInfo) + bodyLen + 1);
    memset((void *)buff, 0, sizeof(struct MsgHeadInfo) + bodyLen + 1);
    memcpy((void *)buff, (void *)&head, sizeof(struct MsgHeadInfo));
    if (bodyLen != 0) {
        PaddingMsgBody(buff + sizeof(struct MsgHeadInfo), msg);
    }
    unsigned int dataLen = sizeof(struct MsgHeadInfo) + bodyLen;
    unsigned int sendLen = 0;
    pthread_mutex_lock(&g_sendLock);
    PrintHeadInfo((struct MsgHeadInfo *)buff, "send message ");
    while (dataLen > sendLen) {
        int retLen = send(GetClientSocket(), buff + sendLen, dataLen - sendLen, 0);
        if (retLen > 0) {
            sendLen += retLen;
        } else {
            pthread_mutex_unlock(&g_sendLock);
            free(buff);
            NOTIFY_LOG_ERROR("send data to server failed %d", (int)errno);
            return -1;
        }
    }
    pthread_mutex_unlock(&g_sendLock);
    free(buff);
    return 0; 
}

static struct AsyncMsgHead *GetAsyncSendMsgHead(NotifyModuleId moduleId, NotifyEvent event, const void *par1,
                                                int par1Len, const void *par2, int par2Len)
{
    struct AsyncMsgHead *head = (struct AsyncMsgHead *)malloc(sizeof(struct AsyncMsgHead));
    if (head == NULL) {
        NOTIFY_LOG_ERROR("malloc failed ret %d", (int)errno);
        return NULL;
    }
    head->destId = moduleId;
    head->event = event;
    head->par1Len = par1Len;
    head->par2Len = par2Len;
    if (head->par1Len == 0) {
        head->par1 = NULL;
    } else {
        head->par1 = malloc(head->par1Len);
        if (head->par1 == NULL) {
            NOTIFY_LOG_ERROR("malloc failed ret %d", (int)errno);
            free(head);
            return NULL;
        }
        memcpy(head->par1, par1, par1Len);
    }
    if (head->par2Len == 0) {
        head->par2 = NULL;
    } else {
        head->par2 = malloc(head->par2Len);
        if (head->par2 == NULL) {
            NOTIFY_LOG_ERROR("malloc failed ret %d", (int)errno);
            free(head);
            free(head->par1);
            return NULL;
        }
        memcpy(head->par2, par2, par2Len);
    }
    return head;
}

static void *AsyncSendMsgHandle(void *arg)
{
    struct AsyncMsgHead *head = (struct AsyncMsgHead *)arg;
    RegisterAsyncFunc proc = GetAsyncFunc(head->destId);
    (void)proc(head->event, head->par1, head->par1Len, head->par2, head->par2Len);
    free(head->par1);
    free(head->par2);
    free(head);
    return NULL;
}

int ReplyMessage(struct MsgHeadInfo *head)
{
    if  (GetClientSocket() < 0) {
        NOTIFY_LOG_ERROR("socket is not ready");
        return -1;
    }
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, ACK_MSG, head->sourceId, head->event, NULL, 0, NULL, 0, -1);
    return SendMsgToServer(&msg, head->seqNum);
}

int PostNotify(NotifyModuleId moduleId, NotifyEvent event, const void *par1, unsigned int par1Len,
               const void *par2, unsigned int par2Len)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return -1;
    }
    /* 同进程回调 */
    if (GetAsyncFunc(moduleId) != NULL) {
        struct AsyncMsgHead *head = GetAsyncSendMsgHead(moduleId, event, par1, par1Len, par2, par2Len);
        if (head == NULL) {
            NOTIFY_LOG_ERROR("create async send head failed");
            return -1;
        }
        if (IsThreadPoolInit()) {
            AddTaskInThreadPool(AsyncSendMsgHandle, (void *)head);
        } else {
            AsyncSendMsgHandle((void *)head);
        }
        return 0;
    }
    if  (GetClientSocket() < 0) {
        NOTIFY_LOG_ERROR("socket is not ready");
        return -1;
    }
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, ASNYC_MSG, moduleId, event, par1, par1Len, par2, par2Len, 0);
    /* 异步 sequence number 无关紧要 */
    return SendMsgToServer(&msg, g_sequenceNum);
}