#include "notify_init_recv.h"
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
#include "notify_init_send.h"
#include "notify_client_common.h"

#define ONCE_READ_SIZE 1024

static int RecvMsg(void *buff, size_t len)
{
    size_t restLen = len;
    while (restLen > 0) {
        size_t readLen = restLen >= ONCE_READ_SIZE ? ONCE_READ_SIZE : restLen;
        ssize_t ret = recv(GetClientSocket(), buff + len - restLen, readLen, 0);
        if (ret < 0) {
            NOTIFY_LOG_ERROR("read message faild ret %d", (int)errno);
            return -1;
        }
        restLen -= ret;
    }
    return 0;
}

static int RecvMsgHead(struct MsgHeadInfo *head)
{
    return RecvMsg(head, sizeof(struct MsgHeadInfo));
}

static int RecvMsgBody(char *buff, int len)
{
    return RecvMsg(buff + sizeof(struct MsgHeadInfo), len);
}

static struct MsgHeadInfo *RecvMsg(void)
{
    struct MsgHeadInfo head;
    memset((void *)&head, 0, sizeof(struct MsgHeadInfo));
    if (RecvMsgHead(&head)) {
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

static void *AsyncMsgHandle(void *arg)
{
    /*
        if(isRegister && noack) {
            registerFunc
        } else if (server ack err) {

        }
    */
    struct MsgHeadInfo *head = (struct MsgHeadInfo *)arg;
    free(head);
    return NULL;
}

static void *SyncMsgHandle(void *arg)
{
    /*
        if(isRegister) {
            registerFunc
            send ack
        }
    */
    struct MsgHeadInfo *head = (struct MsgHeadInfo *)arg;
    free(head);
    return NULL;
}

static void *AckMsgHandle(void *arg)
{
    /*
        HandleAckMsg{
            save ackmsg
            wake waitackfunc
        } 
    */
}

static void *NotifyClientRecv(void *UNUSED(arg))
{
    fd_set rfds;
    while (1) {
        if (GetClientSocket() < 0) break;
        FD_ZERO(&rfds);
        FD_SET(GetClientSocket(), &rfds);
        int ret = select(GetClientSocket() + 1, &rfds, NULL, NULL, NULL);
        if (ret < 0) {
            NOTIFY_LOG_ERROR("select failed ret = %d", (int)errno);
            break;
        }
        if (FD_ISSET(GetClientSocket(), &rfds) == 0) {
            continue;
        }
        struct MsgHeadInfo *head = RecvMsg();
        if (head == NULL) {
            NOTIFY_LOG_ERROR("read message failed");
            break;
        }
        if (head->syncType == 1) {
            if (IsThreadPoolInit()) {
                AddTaskInThreadPool(AsyncMsgHandle, head);
            } else {
                AsyncMsgHandle(head);
            }
            continue;
        }
/*      同步暂不实现
        if (head->ackType == ACK_MSG) {
            continue;
        }
        if (IsThreadPoolInit()) {
            AddTaskInThreadPool(SyncMsgHandle, head);
        } else {
            SyncMsgHandle(head);
        }
*/
    }
    return NULL;
}

int CreateRecvThread(void)
{
    pthread_t tid;
    if (pthread_create(&tid, NULL, (void *)NotifyClientRecv, NULL) != 0) {
        NOTIFY_LOG_ERROR("create thread faild ret %d", (int)errno);
        return 0;
    }
    return -1;
}