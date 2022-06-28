#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "notify.h"

/* 同进程测试 */

struct syncMsg {
    NotifyEvent event;
    int (*func)(const void *input, unsigned int inLen, void *output, unsigned int outLen);
};

struct asyncMsg {
    NotifyEvent event;
    int (*func)(const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len);
};

int test2event1SyncFunc(const void *input, unsigned int inLen, void *output, unsigned int outLen)
{
    char *temp = "send sync test";
    printf("test2 event test1 sync msg\n");
    if (input != NULL) {
        printf("%s\n", (char *)input);
    }
    if (output != NULL) {
        memcpy(output, temp, strlen(temp));
        char *temp = (char *)output;
        temp[strlen(temp)] = '\0';
    }
    return 0;
}

int test2event2SyncFunc(const void *input, unsigned int inLen, void *output, unsigned int outLen)
{
    char *temp = "this test2 sync func";
    printf("test1 event test2 sync msg\n");
    if (input != NULL) {
        printf("%s\n", (char *)input);
    }
    if (output != NULL) {
        memcpy(output, temp, strlen(temp));
        char *temp = (char *)output;
        temp[strlen(temp)] = '\0';
    }  
    return 0;
}

int test2event1AsyncFunc(const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len)
{
    printf("event1 recive async msg\n");
    if (par1 != NULL) {
        printf("%s\n", (char *)par1);
    }
    if (par2 != NULL) {
        printf("%s\n", (char *)par2);
    }
    return 0;
}

int test2event2AsyncFunc(const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len)
{
    printf("event2 recive async msg\n");
    if (par1 != NULL) {
        printf("%s\n", (char *)par1);
    }
    if (par2 != NULL) {
        printf("%s\n", (char *)par2);
    }
    PostNotify(MODULE_TEST1, MODULE_TEST1_EVENT2, par1, par1Len, "this test2 func", strlen("this test2 func"));

    return 0;
}

struct syncMsg g_syncMsg[2] = {
    {MODULE_TEST2_EVENT1, test2event1SyncFunc},
    {MODULE_TEST2_EVENT2, test2event2SyncFunc},
};

struct asyncMsg g_asyncMsg[2] = {
    {MODULE_TEST2_EVENT1, test2event1AsyncFunc},
    {MODULE_TEST2_EVENT2, test2event2AsyncFunc},

};

int Test2ReciveSyncMsg(NotifyEvent event, const void *input, unsigned int inLen, void *output, unsigned int outLen)
{
    for (int i = 0; i < sizeof(g_syncMsg) / sizeof(g_syncMsg[0]); i++) {
        if (event != g_syncMsg[i].event) continue;
        return g_syncMsg[i].func(input, inLen, output, outLen);
    }
    return -1;
}

int Test2ReciveAsyncMsg(NotifyEvent event, const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len)
{
    for (int i = 0; i < sizeof(g_asyncMsg) / sizeof(g_asyncMsg[0]); i++) {
        if (event != g_asyncMsg[i].event) continue;
        return g_asyncMsg[i].func(par1, par1Len, par2, par2Len);
    }
    return -1;
}

int main(void)
{
    char temp[20];
    char recv[200];
    char send[200] = "main data";
    pthread_t tid;
    NotifyInit();
    RegisterNotifyFunction(MODULE_TEST2, Test2ReciveAsyncMsg, Test2ReciveSyncMsg);
    fgets(temp, sizeof(temp), stdin); //敲回车触发
    printf("start test2 \n");
    while (1) {
        fgets(temp, sizeof(temp), stdin); //敲回车触发
        SendNotify(MODULE_TEST1, MODULE_TEST1_EVENT1, send, strlen(send) + 1, recv, sizeof(recv));
        PostNotify(MODULE_TEST1, MODULE_TEST1_EVENT2, recv, sizeof(recv), send, sizeof(send));
        printf("read data from module1, %s\n", recv);
        fgets(temp, sizeof(temp), stdin); //敲回车触发
            printf("send %s to module2 event2\n", recv);
        SendNotify(MODULE_TEST1, MODULE_TEST1_EVENT2, send, sizeof(send), recv, sizeof(recv));
        PostNotify(MODULE_TEST1, MODULE_TEST1_EVENT1, recv, strlen(recv) + 1, send, strlen(send) + 1);
        printf("read data from module1, %s\n", recv);
    }
    return 0;
}
