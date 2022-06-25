#include "notify_server_init.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>  
#include <sys/epoll.h>  
#include <sys/socket.h>  
#include <sys/un.h>
#include <signal.h>
#include "notify.h"
#include "notify_server_common.h"
#include "notify_server_recv.h"
#include "notify_server_send.h"



static int g_IsNotifyServerInit = 0;
static int g_socketFd = -1;
static int g_moudleId[MODULE_ID_MAX] = {0};
static int g_epollFd = -1;

void CloseServerSocket(void)
{
    close(g_socketFd);
    g_socketFd = -1;
}

int GetServerSocket(void)
{
    return g_socketFd;
}

int GetEpollFd(void)
{
    return g_epollFd;
}

static int CreateServerSocket(void)
{
    struct sockaddr_un addr;
    memset((void *)&addr, 0, sizeof(addr));
    g_socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socketFd < 0) {
        NOTIFY_LOG_ERROR("cannot create client socket");  
        return -1;  
    }
    addr.sun_family = AF_UNIX;
    int ret = snprintf(addr.sun_path, sizeof(addr.sun_path), SERVER_PATH);
    if(ret < 0) {
        NOTIFY_LOG_ERROR("snprintf error ret %d", (int)errno);
        CloseServerSocket();
        return -1;
    }
    size_t len = offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path);
    unlink(addr.sun_path);
    ret = bind(g_socketFd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        NOTIFY_LOG_ERROR("bind faild ret %d", (int)errno);
        CloseServerSocket();
        return -1;
    }
    return 0;
}

int CreateServerMonitor(void)
{
    struct epoll_event serverEp;
    g_epollFd = epoll_create(MODULE_ID_MAX);
	if (g_epollFd < 0) {
        NOTIFY_LOG_ERROR("epoll_create faild ret %d", (int)errno);
        return -1;
    }
	serverEp.events = EPOLLIN;
    serverEp.data.fd = g_socketFd;
	if (epoll_ctl(g_epollFd, EPOLL_CTL_ADD, g_socketFd, &serverEp) < 0) {
        NOTIFY_LOG_ERROR("add monitor server fd faild ret %d", (int)errno);
        close(g_epollFd);
        g_epollFd = -1;
        return -1;
    }
}

int NotifyServerInit(void)
{
    if (g_IsNotifyServerInit == 1) {
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    if (CreateServerSocket() < 0) {
        NOTIFY_LOG_ERROR("Create Client Socket failed");
        return -1;
    }
    if(listen(g_socketFd, MODULE_ID_MAX) < 0) {  
        NOTIFY_LOG_ERROR("cannot listen the client connect request");  
        CloseServerSocket();
        return 1;  
    }
    if (CreateServerMonitor() < 0) {
        NOTIFY_LOG_ERROR("CreateServerMonitor failed");
        CloseServerSocket();
        return -1;
    }
    if (CreateRecvThread() < 0) {
        NOTIFY_LOG_ERROR("Create recive thread failed");
        CloseServerSocket();
        return -1;
    }
    if (InitMessageList() < 0) {
        NOTIFY_LOG_ERROR("Init Message List failed");
        CloseServerSocket();
        return -1;
    }
    NOTIFY_LOG_INFO("init success");
    g_IsNotifyServerInit = 1;
    return 0;
}