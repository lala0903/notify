#include "notify_init_init.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/un.h>  
#include "threadpool.h"
#include "notify_init_common.h"


int g_IsNotifyClientInit = 0;
int g_socketFd = -1;

static void CloseClientSocket(void)
{
    close(g_socketFd);  
    g_socketFd = -1;
}

static int GetClientSocket(void)
{
    return g_socketFd;
}

static int CreateClientSocket(void)
{
    struct sockaddr_un addr;
    memset((void *)&addr, 0, sizeof(addr));
    g_socketFd = socket(PF_UNIX, SOCK_STREAM, 0);  
    if (g_socketFd < 0) {
        NOTIFY_LOG_ERROR("cannot create client socket");  
        return -1;  
    }     
    addr.sun_family = AF_UNIX;
    int ret = snprintf(addr.sun_path, sizeof(addr.sun_path), "notify_client%d", (int)getpid());
    if(ret < 0) {
        NOTIFY_LOG_ERROR("snprintf error ret %d", (int)errno);  
        CloseClientSocket();
        return -1;  
    } 
    ret = connect(g_socketFd, (struct sockaddr *)&addr, sizeof(addr));  
    if(ret == -1) {
        NOTIFY_LOG_ERROR("cannot connect to the server");  
        CloseClientSocket();
        return -1;
    }
    return 0;
}

int NotifyInit(void)
{
    if (g_IsNotifyClientInit == 1) {
        return -1;
    }
    if (CreateClientSocket() < 0) {
        NOTIFY_LOG_ERROR("Create Client Socket failed");
        return -1;
    }
    if (InitThreadPool(3) < 0) {
        NOTIFY_LOG_ERROR("Create thread pool failed");
        CloseClientSocket();
        return -1;
    }
    if (CreateRecvThread() < 0) {
        NOTIFY_LOG_ERROR("Create recive thread failed");
        CloseClientSocket();
        DestroyThreadPool();
        return -1;
    }
    NOTIFY_LOG_INFO("init success");  
    g_IsNotifyClientInit = 1;
    return 0;
}