#include "notify_init_init.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "threadpool.h"

void NotifyClientRecv(void *UNUSED(arg))
{
    while (1) {
        // 读一包消息

        // 处理一包消息
    }
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