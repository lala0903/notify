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

void DataLock(void)
{
    pthread_mutex_lock(&g_dateLock);
}

void DataUnlock(void)
{
    pthread_mutex_unlock(&g_dateLock);
}

void WakeupDataWaite(void)
{
    pthread_cond_broadcast(&g_dateWait);
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
    if (g_currentMoudleId == 0) {
        g_currentMoudleId = moduleId;
    }
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, REG_MSG, MODULE_NOTIFY_SERVER, 0,
                        (void*)&g_currentMoudleId, sizeof(g_currentMoudleId), NULL, 0, -1);
    /* 异步 sequence number 紧要 */
    return SendMsgToServer(&msg, g_sequenceNum);
}

int RegisterNotifyFunction(NotifyModuleId moduleId, RegisterAsyncFunc asyncFunc, RegisterSyncFunc syncFunc)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return -1;
    }
    if (asyncFunc != NULL) {
        g_asyncFunc[moduleId] = asyncFunc;
    }
    if (syncFunc != NULL) {
        g_syncFunc[moduleId] = syncFunc;
    }
    if (RegisterToServer(moduleId) == 0) {
        return 0;
    }
    NOTIFY_LOG_ERROR("register module to server failed");
    g_asyncFunc[moduleId] = NULL;
    g_syncFunc[moduleId] = NULL;
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

RegisterAsyncFunc GetSyncFunc(NotifyModuleId moduleId)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return NULL;
    }
    return g_syncFunc[moduleId];
}

void UnregisterNotifyFunction(NotifyModuleId moduleId)
{
    if (IS_MODULEID_INVAILD(moduleId)) {
        NOTIFY_LOG_ERROR("moduleId is invalid %d", moduleId);
        return;
    }
    g_asyncFunc[moduleId] = NULL;
    g_syncFunc[moduleId] = NULL;
}

static int SendSyncMsg(const struct SendMsgFrame *msg)
{
    pthread_mutex_lock(&g_seqNumLock);
    unsigned int seqNum = g_sequenceNum++;
    pthread_mutex_unlock(&g_seqNumLock);
    int ret = SendMsgToServer(&msg, seqNum);
    pthread_mutex_lock(&g_dateLock);
    int retValue = -1;
    g_DestMoudleId[msg->destId] = seqNum;
    while (1) {
        if (IsNodeExistAckList(seqNum) == 0) {
            GetDataFromeAckList(seqNum, msg->par2, msg->par2Len, &retValue);
            RemoveNodeFromAckList(seqNum);
            pthread_mutex_unlock(&g_dateLock);
            break;
        }
        pthread_cond_wait(&g_dateWait, &g_dateLock);
        if (g_DestMoudleId[msg->destId] == -1) {
            pthread_mutex_unlock(&g_dateLock);
            break;
        }
    }
    return retValue;
}

int SendNotify(NotifyModuleId moduleId, NotifyEvent event, const void *input, int inLen, void *output, int outLen)
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
        bodyLen = msg->par1Len + msg->par2Len;
        head->totalLen = bodyLen;
        head->par1Len = msg->par1Len;
        head->par2Len = msg->par2Len;
        break;
    case SNYC_MSG:
        bodyLen = msg->par1Len + msg->par2Len;
        head->totalLen = bodyLen;
        head->par1Len = msg->par1Len;
        head->outLen = msg->par2Len;
        break;
    case REG_MSG:
        bodyLen = 0;
        break;
    case ACK_MSG:
        bodyLen = msg->par2Len;
        head->outLen = msg->par2Len;
        break;
    default:
        break;
    }
    return bodyLen;
}

static void PaddingMsgBody(void *buff, const struct SendMsgFrame *msg)
{
    switch (msg->msgType) {
    case ASNYC_MSG:
        memcpy(buff, msg->par1, msg->par1Len);
        memcpy(buff + msg->par1Len, msg->par2, msg->par2Len);
        break;
    case SNYC_MSG:
        memcpy(buff, msg->par1, msg->par1Len);
        memcpy(buff + msg->par1Len, msg->par1, msg->par1Len);
        break;
    case REG_MSG:
        break;
    case ACK_MSG:
        memcpy(buff, msg->par2, msg->par2Len);
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
    if (msg->msgType == ASNYC_MSG || (msg->msgType == REG_MSG) {
        head->syncType = ASYNC_TYPE;
    } else {
        head->syncType = SYNC_TYPE;
    }
}

int SendMsgToServer(const struct SendMsgFrame *msg, unsigned int seqNum)
{
    struct MsgHeadInfo head;
    memset((void *)&head, 0, sizeof(head));
    PaddingMsgHead(msg, &head);
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
    while (dataLen > sendLen) {
        int retLen = send(GetClientSocket(), buff + sendLen, dataLen - sendLen);
        if (retLen > 0) {
            sendLen += retLen;
        } else {
            pthread_mutex_unlock(&g_sendLock);
            free(buff);
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

int PostNotify(NotifyModuleId moduleId, NotifyEvent event, const void *par1, int par1Len,
               const void *par2, int par2Len)
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
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, ASNYC_MSG, moduleId, event, par1, par1Len, par2, par2Len, 0);
    /* 异步 sequence number 无关紧要 */
    return SendMsgToServer(&msg, g_sequenceNum);
}