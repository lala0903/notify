
#include "notify_init_send.h"
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

static int g_moudleId[MODULE_ID_MAX] = {0};
static RegisterAsyncFunc g_asyncFunc[MODULE_ID_MAX] = {NULL};
static RegisterSyncFunc g_syncFunc[MODULE_ID_MAX] = {NULL};

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
    return 0;
}

static RegisterAsyncFunc GetAsyncFunc(NotifyModuleId moduleId)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return NULL;
    }
    return g_asyncFunc[moduleId];
}

static RegisterAsyncFunc GetSyncFunc(NotifyModuleId moduleId)
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

static int SendSyncMsg()
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
    if (RegisterSyncFunc(moduleId) != NULL) {
        // 打包消息
        RegisterSyncFunc proc = RegisterSyncFunc(moduleId);
    }
    // 打包消息，发送给server

    SendSyncMsg();
}

static int SendMsg()
{

}

int PostNotify(NotifyModuleId moduleId, NotifyEvent event, const void *par1, int par1Len, const void *par2, int par2Len)
{
    if (moduleId >= MODULE_ID_MAX) {
        NOTIFY_LOG_ERROR("moduleId is invalid %u", moduleId);
        return -1;
    }
    /* 同进程回调 */
    if (RegisterAsyncFunc(moduleId) != NULL) {
        // 打包消息
        RegisterAsyncFunc proc = RegisterAsyncFunc(moduleId);
        if (IsThreadPoolInit()) {
            AddTaskInThreadPool();
        } else {

        }
        return 0;
    }
    // 打包消息，发送给server

    SendMsg();
}