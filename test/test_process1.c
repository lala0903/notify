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

int test1event1SyncFunc(const void *input, unsigned int inLen, void *output, unsigned int outLen)
{
    char *temp = "send sync test";
    printf("test1 event test1 sync msg\n");
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

int test1event2SyncFunc(const void *input, unsigned int inLen, void *output, unsigned int outLen)
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

int test1event1AsyncFunc(const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len)
{
    printf("test1 event test1 async msg\n");
    if (par1 != NULL) {
        printf("%s\n", (char *)par1);
    }
    if (par2 != NULL) {
        printf("%s\n", (char *)par2);
    }
    return 0;
}

int test1event2AsyncFunc(const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len)
{
    printf("test1 event test2 async msg\n");
    if (par1 != NULL) {
        printf("%s\n", (char *)par1);
    }
    if (par2 != NULL) {
        printf("%s\n", (char *)par2);
    } 
    return 0;
}

struct syncMsg g_syncMsg[2] = {
    {MODULE_TEST1_EVENT1, test1event1SyncFunc},
    {MODULE_TEST1_EVENT2, test1event2SyncFunc},
};

struct asyncMsg g_asyncMsg[2] = {
    {MODULE_TEST1_EVENT1, test1event1AsyncFunc},
    {MODULE_TEST1_EVENT2, test1event2AsyncFunc},

};

int Test1ReciveSyncMsg(NotifyEvent event, const void *input, unsigned int inLen, void *output, unsigned int outLen)
{
    for (int i = 0; i < sizeof(g_syncMsg) / sizeof(g_syncMsg[0]); i++) {
        if (event != g_syncMsg[i].event) continue;
        return g_syncMsg[i].func(input, inLen, output, outLen);
    }
    return -1;
}

int Test1ReciveAsyncMsg(NotifyEvent event, const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len)
{
    for (int i = 0; i < sizeof(g_asyncMsg) / sizeof(g_asyncMsg[0]); i++) {
        if (event != g_asyncMsg[i].event) continue;
        return g_asyncMsg[i].func(par1, par1Len, par2, par2Len);
    }
    return -1;
}

int Test2ReciveSyncMsg(NotifyEvent event, const void *input, unsigned int inLen, void *output, unsigned int outLen)
{
    char send[200] = {0};
    printf("Sync Test2 recive envent %d %s\n", (int)event - MODULE_TEST1_EVENT1, (char *)input);
    sprintf(send, "lala %d", event);
    memcpy(output, send, strlen(send) + 1);
    return 0;
}

int Test2ReciveAsyncMsg(NotifyEvent event, const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len)
{
    printf("async Test2 recive envent %d\n", (int)event - MODULE_TEST1_EVENT1);
    printf("par1 %s   par2 %s\n", (char*)par1, (char*)par2);
    return 0;
}


int main(void)
{
    char temp[20];
    char recv[200];
    char send[200] = "process test1";
    pthread_t tid;
    NotifyInit();
    RegisterNotifyFunction(MODULE_TEST1, Test1ReciveAsyncMsg, Test1ReciveSyncMsg);
    SendNotify(MODULE_NOTIFY_SERVER, SERVER_WRITE_SEND_TEST, send, strlen(send), NULL, 0);
    printf("start test1 \n");
    while(1) {
        fgets(temp, sizeof(temp), stdin); //敲回车触发
        printf("send %s to module2 event1\n", send);
        SendNotify(MODULE_TEST2, MODULE_TEST2_EVENT1, send, sizeof(send), recv, sizeof(recv));
        PostNotify(MODULE_TEST2, MODULE_TEST2_EVENT2, recv, sizeof(recv), send, sizeof(send));
        // PostNotify(MODULE_TEST2, MODULE_TEST2_EVENT2, send, strlen(send), NULL, 0);
        printf("get msg from module2 %s\n", recv);
        fgets(temp, sizeof(temp), stdin);
        printf("send %s to module2 event2\n", recv);
        SendNotify(MODULE_TEST2, MODULE_TEST2_EVENT2, send, sizeof(send), recv, sizeof(recv));
        PostNotify(MODULE_TEST2, MODULE_TEST2_EVENT1, recv, sizeof(recv), send, sizeof(send));
        printf("get msg from module2 %s\n", recv);
    }
    fgets(temp, sizeof(temp), stdin); //敲回车触发
    exit(0);
    return 0;
}
