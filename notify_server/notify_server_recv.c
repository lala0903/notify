#include "notify_server_recv.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <sys/epoll.h>  
#include <sys/socket.h>  
#include <sys/un.h>   
#include <sys/time.h>
#include <sys/types.h>
#include "notify.h"
#include "notify_server_init.h"
#include "notify_server_send.h"
#include "notify_server_common.h"

#define ONCE_READ_SIZE 1024

static int g_recvThreadRun = 0;
static struct epoll_event g_epollEvent[MODULE_ID_MAX];

/* 
    read返回0，对方正常调用close关闭链接
    read返回-1，需要通过errno来判断，如果不是EAGAIN和EINTR，那么就是对方异常断开链接
    两种情况服务端都要close套接字
*/
void RemoveClientListen(int clientFd)
{
    if (clientFd < 0) {
        NOTIFY_LOG_ERROR("parameter fd %d invalid", clientFd);
        return;
    }
    NOTIFY_LOG_INFO("Remove client fd %d", clientFd);
    if (epoll_ctl(GetEpollFd(), EPOLL_CTL_DEL, clientFd, NULL) < 0) {
        NOTIFY_LOG_ERROR("Remove client fd failed ret %d", (int)errno);
    }
    close(clientFd);
}

static int AddClientListen(void)
{
    struct sockaddr_un addr;
    struct epoll_event temp;
    socklen_t len = sizeof(addr);
    int fd = accept(GetServerSocket(), (struct sockaddr *)&addr, &len);
    if (fd < 0) {
        NOTIFY_LOG_ERROR("accept client connect faild ret %d", (int)errno);
        return -1;
    }
    temp.events = EPOLLIN;
    temp.data.fd = fd;
    if (epoll_ctl(GetEpollFd(), EPOLL_CTL_ADD, fd, &temp) < 0) {
        NOTIFY_LOG_ERROR("add client fd int epoll event failed ret %d", (int)errno);
                return -1;
    }
    return 0;
}

static int RecvMsgFromClient(int fd, void *buff, size_t len)
{
    size_t totalLen = 0;
    while (totalLen < len) {
        size_t readLen = len - totalLen >= ONCE_READ_SIZE ? ONCE_READ_SIZE : len - totalLen;
        ssize_t ret = read(fd, buff + totalLen, readLen);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                NOTIFY_LOG_WARN("get signal EAGAIN || EINTR %d", (int)errno);
                continue;
            }
            NOTIFY_LOG_ERROR("read message faild ret %d", (int)errno);
            RemoveClientListen(fd);
            return -1;
        } else if (ret == 0) {
            NOTIFY_LOG_WARN("read message ret = 0");
            RemoveClientListen(fd);
            return totalLen;
        } else {
            totalLen += ret;
        }
    }
    return totalLen;
}

static int RecvMsgHead(int fd, struct MsgHeadInfo *head)
{
    int retLen = RecvMsgFromClient(fd, (void *)head, sizeof(struct MsgHeadInfo));
    if (retLen != sizeof(struct MsgHeadInfo)) {
        NOTIFY_LOG_ERROR("read message head failed");
        return -1;
    }
    return 0;
}

static int RecvMsgBody(int fd, char *buff, size_t len)
{
    int retLen = RecvMsgFromClient(fd, buff + sizeof(struct MsgHeadInfo), len);
    if (retLen != len) {
        NOTIFY_LOG_ERROR("read message body failed");
        return -1;
    }
    return 0;
}

static struct MsgHeadInfo *RecvMsg(int fd)
{
    struct MsgHeadInfo head;
    memset((void *)&head, 0, sizeof(struct MsgHeadInfo));
    if (RecvMsgHead(fd, &head) != 0) {
        NOTIFY_LOG_ERROR("read msg head failed");
        return NULL;
    }
    unsigned int bodyLen = head.totalLen;
    if (bodyLen < 0 || bodyLen > INT_MAX + sizeof(struct MsgHeadInfo) + 1) {
        NOTIFY_LOG_ERROR("bodyLen is invalid %d", bodyLen);
        return NULL;
    }
    char *buff = (char *)malloc(bodyLen + sizeof(struct MsgHeadInfo) + 1);
    if (buff == NULL) {
        NOTIFY_LOG_ERROR("malloc faild ret %d", (int)errno);
        return NULL;
    }
    memcpy((void *)buff, (void *)&head, sizeof(struct MsgHeadInfo));
    if (bodyLen == 0) {
        NOTIFY_LOG_ERROR("bodyLen == 0");
        return (struct MsgHeadInfo *)buff;
    }
    if (RecvMsgBody(fd, buff, (size_t)bodyLen) != 0) {
        free(buff);
        NOTIFY_LOG_ERROR("padding msg body failed");
        return NULL;
    }
    return (struct MsgHeadInfo *)buff;
}

static int ReadDataFromClientHandle(int fd)
{
    struct MsgHeadInfo *buff = RecvMsg(fd);
    if (buff == NULL) {
        return -1;
    }
    return AddMessageInList(buff, fd);
}

static void *NotifyServerRecv(void *arg)
{
    const size_t eventSize = sizeof(g_epollEvent) / sizeof(g_epollEvent[0]);
    int clientCnt;
    int j = 0;
    while (g_recvThreadRun == 1) {
        int readyCnt = epoll_wait(GetEpollFd(), g_epollEvent, eventSize, -1); /* 阻塞监听 */
        if (readyCnt < 0) {
            NOTIFY_LOG_ERROR("epoll_wait faild ret %d", (int)errno);
            break;
        }
        for (int i = 0; i < readyCnt; i++) {
            if (!(g_epollEvent[i].events & EPOLLIN)) {
                NOTIFY_LOG_WARN("recive a invaild event");
                continue;
            }
            /* 客户端消息 */
            if (g_epollEvent[i].data.fd != GetServerSocket()) {
                ReadDataFromClientHandle(g_epollEvent[i].data.fd);
            } else {
                NOTIFY_LOG_INFO("new client connect");
                AddClientListen();
            }
        }
    }
    return NULL;
}

int CreateRecvThread(void)
{
    pthread_t tid;
    g_recvThreadRun = 1;
    if (pthread_create(&tid, NULL, (void *)NotifyServerRecv, NULL) != 0) {
        NOTIFY_LOG_ERROR("create thread faild ret %d", (int)errno);
        g_recvThreadRun = 0;
        return -1;
    }
    return 0;
}