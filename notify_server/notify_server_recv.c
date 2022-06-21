#include "notify_server_recv.h"
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
#include "notify_init_send.h"

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

static void *NotifyServerRecv(void *arg)
{
    fd_set rfds;
    int fd = *(int *)arg;
    while (g_recvThreadRun == 1) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        int ret = select(fd + 1, &rfds, NULL, NULL, NULL);
        if (ret > 0) {
            struct MsgHeadInfo *head = RecvMsg();
            if (head == NULL) {
                NOTIFY_LOG_ERROR("read message failed");
                break;
            }
            // 处理一包数据
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