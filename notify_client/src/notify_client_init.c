#include "notify_client_init.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/un.h>
#include <signal.h>
#include "threadpool.h"
#include "notify_client_common.h"
#include "notify_client_ack_list.h"
#include "notify_client_send.h"
#include "notify_client_recv.h"
#include "notify_client_time.h"

static int g_IsNotifyClientInit = 0;
static int g_socketFd = -1;

bool IsNOtifyInit(void)
{
    return g_IsNotifyClientInit == 1;
}

void CloseClientSocket(void)
{
    if (g_socketFd < 0) {
        return;
    }
    for (int i = 0; i < MODULE_ID_MAX; i++) {
        ClearDestModuleIdValue(i);
    }
    close(g_socketFd);
    g_socketFd = -1;
    DataLock();
    WakeupDataWaite();
    DataUnlock();
}

static void clearSocket(void)
{
    if (g_socketFd < 0) {
        return;
    }
    close(g_socketFd);
    g_socketFd = -1;
}

int GetClientSocket(void)
{
    return g_socketFd;
}

static int CreateClientSocket(void)
{
    struct sockaddr_un addr;
    memset((void *)&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    int ret = snprintf(addr.sun_path, sizeof(addr.sun_path), CLIENT_PATH"%05d", (int)getpid());
    if(ret < 0) {
        NOTIFY_LOG_ERROR("snprintf error ret %d", (int)errno);  
        return -1;  
    }
    size_t len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    unlink(addr.sun_path);
    g_socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socketFd < 0) {
        NOTIFY_LOG_ERROR("cannot create client socket");
        return -1;
    }
    ret = bind(g_socketFd, (struct sockaddr *)&addr, len);
    if(ret < 0) {
        NOTIFY_LOG_ERROR("bind faild ret %d", (int)errno);
        clearSocket();
        return -1;
    }
    return 0;
}

int ConnectNotifyServer(void)
{
    struct sockaddr_un addr;
    memset((void *)&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    int ret = snprintf(addr.sun_path, sizeof(addr.sun_path), SERVER_PATH);
    if(ret < 0) {
        NOTIFY_LOG_ERROR("snprintf error ret %d", (int)errno);  
        return -1;  
    }
    if (CreateClientSocket() < 0) {
        NOTIFY_LOG_ERROR("Create Client Socket failed");
        return -1;
    }
    size_t len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    ret = connect(g_socketFd, (struct sockaddr *)&addr, len);  
    if(ret == -1) {
        NOTIFY_LOG_ERROR("cannot connect to the server");  
        clearSocket();
        return -1;
    }
    return 0;
}

/* 初始化流程和注销流程顺序勿随意改动 */
int NotifyInit(void)
{
    if (g_IsNotifyClientInit == 1) {
        NOTIFY_LOG_WARN("notify is initialized");
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    if (NotifyAckLisitInit() < 0) {
        NOTIFY_LOG_ERROR("Create ack list failed");
        goto EXIT;
    }
    if (CreateTimeOutListenerThread() < 0) {
        NOTIFY_LOG_ERROR("Create Time Out Listener failed");
        goto EXIT;
    }
    if (InitThreadPool(3) < 0) {
        NOTIFY_LOG_ERROR("Create thread pool failed");
        goto EXIT;
    }
    if (ConnectNotifyServer() < 0) {
        NOTIFY_LOG_ERROR("Create Client Socket failed");
        goto EXIT;
    }
    if (CreateRecvThread() < 0) {
        NOTIFY_LOG_ERROR("Create recive thread failed");
        goto EXIT;
    }
    NOTIFY_LOG_INFO("init success");  
    g_IsNotifyClientInit = 1;
    return 0;
EXIT:
    NotifyAckLisitDestroy();
    DestoryTimeOutListenerThread();
    DestoryRecvThread();
    DestroyThreadPool();
    clearSocket();
    return -1;
}

void NotifyDestroy(void)
{
    if (g_IsNotifyClientInit == 0) {
        return;
    }
    UnregisterNotifyModule();
    DestoryTimeOutListenerThread();
    NotifyAckLisitDestroy();
    DestoryRecvThread();
    DestroyThreadPool();
    CloseClientSocket();
    NOTIFY_LOG_INFO("destroy success");  
    g_IsNotifyClientInit = 0;
    return;
}