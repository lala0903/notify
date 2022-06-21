
#include "notify_client_send.h"
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
#include "notify_client_common.h"

// static int g_moudleId[MODULE_ID_MAX] = {0};
static int g_currentMoudleId = 0; /* 同一进程使用第一个注册的moduleid 与server通信 */
static RegisterAsyncFunc g_asyncFunc[MODULE_ID_MAX] = {NULL};
static RegisterSyncFunc g_syncFunc[MODULE_ID_MAX] = {NULL};
static unsigned int g_sequenceNum = 0; //同步消息用于接收数据，异步无用

static pthread_mutex_t g_dateLock;
static pthread_cond_t g_dateWait;

void NotifyClientSendInit(void)
{
    pthread_mutex_init(&g_dateLock, NULL);
    pthread_cond_init(&g_dateWait, NULL);
}

unsigned int GetSequenceNum(void)
{
    return g_sequenceNum;
}

static int RegisterToServer(NotifyModuleId moduleId)
{
    if (g_currentMoudleId == 0) {
        g_currentMoudleId = moduleId;
    }
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, REG_MSG, MODULE_NOTIFY_SERVER, 0, NULL, 0, NULL, 0, -1);
    return SendMsgToServer(&msg, g_sequenceNum++);
}

int RegisterNotifyFunction(NotifyModuleId moduleId, RegisterAsyncFunc asyncFunc, RegisterSyncFunc syncFunc)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return -1;
    }
    if (asyncFunc != NULL) {
        g_asyncFunc[moduleId] = asyncFunc;
    }
    if (syncFunc != NULL) {
        g_syncFunc[moduleId] = syncFunc;
    }
    if (RegisterToServer(moduleId) != 0) {
        NOTIFY_LOG_ERROR("register module failed");
    }
    return 0;
}

RegisterAsyncFunc GetAsyncFunc(NotifyModuleId moduleId)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return NULL;
    }
    return g_asyncFunc[moduleId];
}

RegisterAsyncFunc GetSyncFunc(NotifyModuleId moduleId)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return NULL;
    }
    return g_syncFunc[moduleId];
}

int UnregisterNotifyFunction(NotifyModuleId moduleId)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return -1;
    }
    g_asyncFunc[moduleId] = NULL;
    g_syncFunc[moduleId] = NULL;
    return 0;
}

static int SendSyncMsg(const struct SendMsgFrame *msg)
{
    /*
        sendmsgsync{
            setwaittimeout
            Packing
            send
            waitack
            find ackmsg
        }
    */

}

int SendNotify(NotifyModuleId moduleId, NotifyEvent event, const void *input, int inLen, void *output, int outLen)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return -1;
    }
    /* 同进程回调 */
    RegisterSyncFunc proc = GetSyncFunc(moduleId);
    if (proc != NULL) {
        return proc(event, input, inLen, output, outLen);
    }
    // 打包消息，发送给server
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, SNYC_MSG, moduleId, event, input, inLen, output, outLen, 0);
    SendSyncMsg(&msg);
}

unsigned int GetMsgBodyLen(const struct SendMsgFrame *msg, struct MsgHeadInfo *head)
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

void PaddingMsgBody(void *buff, const struct SendMsgFrame *msg)
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

int SendMsgToServer(const struct SendMsgFrame *msg, unsigned int sequenceNum)
{
    struct MsgHeadInfo head;
    memset((void *)&head, 0, sizeof(head));
    head.seqNum = sequenceNum;
    head.sourceId = g_currentMoudleId;
    head.msgType = msg->msgType;
    head.destId = msg->destModuleId;
    head.event = msg->event;
    head.retValue = msg->retValue;
    if (msg->msgType == ACK_MSG) {
        head.ackType = ACK_OK;
    }
    if (msg->msgType == ASNYC_MSG) {
        head.syncType = ASYNC_TYPE;
    } else {
        head.syncType = SYNC_TYPE;
    }
    unsigned int bodyLen = GetMsgBodyLen(msg, &head);
    char *buff = (char *)malloc(sizeof(struct MsgHeadInfo) + bodyLen + 1);
    memset((void *)buff, 0, sizeof(struct MsgHeadInfo) + bodyLen + 1);
    memcpy((void *)buff, (void *)&head, sizeof(struct MsgHeadInfo));
    if (bodyLen != 0) {
        PaddingMsgBody(buff + sizeof(struct MsgHeadInfo), msg);
    }
    unsigned int dataLen = sizeof(struct MsgHeadInfo) + bodyLen;
    unsigned int sendLen = 0;
    while (dataLen > sendLen) {
        int retLen = send(GetClientSocket(), buff + sendLen, dataLen - sendLen);
        if (retLen > 0) {
            sendLen += retLen;
        } else {
            free(buff);
            return -1;
        }
    }
    free(buff);
    return 0; 
}

int PostNotify(NotifyModuleId moduleId, NotifyEvent event, const void *par1, int par1Len, const void *par2, int par2Len)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return -1;
    }
    /* 同进程回调 */
    if (GetAsyncFunc(moduleId) != NULL) {
        if (IsThreadPoolInit()) {
            AddTaskInThreadPool();
        } else {

        }
        return 0;
    }
    // 打包消息，发送给server
    struct SendMsgFrame msg;
    INIT_SEND_MSG_FRAME(msg, ASNYC_MSG, moduleId, event, par1, par1Len, par2, par2Len, 0);
    return SendMsgToServer(&msg, g_sequenceNum);
}