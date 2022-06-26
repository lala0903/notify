#ifndef __NOTIFY_H__
#define __NOTIFY_H__

#include "notify_common.h"

/* socket通信用文件，根据实际情况修改 */
#define SERVER_PATH "/home/lala0903/c_learn/notify_server"
#define CLIENT_PATH "/home/lala0903/c_learn/notify_client"

#define SYNC_TYPE 0
#define ASYNC_TYPE 1

typedef enum{
    ASNYC_MSG,
    SNYC_MSG,
    REG_MSG,
    ACK_MSG,
} NotifyMsgType;

typedef enum{
    NO_ACK,
    ACK_OK,
    ACK_ERR,
} NotifyMsgAskType;

struct MsgHeadInfo {
    unsigned int seqNum;
    unsigned int sourceId;
    unsigned int msgType;
    unsigned int destId;
    unsigned int event;
    unsigned int syncType;
    NotifyMsgAskType ackType;
    unsigned int totalLen;
    unsigned int par1Len;
    unsigned int par2Len;
    unsigned int outLen;
    int retValue;
};

typedef int (*RegisterSyncFunc)(NotifyEvent event, const void *input, unsigned int inLen, void *output, unsigned int outLen);
typedef int (*RegisterAsyncFunc)(NotifyEvent event, const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len);

int NotifyInit(void);
void NotifyDestroy(void);
int RegisterNotifyFunction(NotifyModuleId moduleId, RegisterAsyncFunc asyncFunc, RegisterSyncFunc syncFunc);
int SendNotify(NotifyModuleId moduleId, NotifyEvent event, const void *input, unsigned int inLen, void *output, unsigned int outLen);
int PostNotify(NotifyModuleId moduleId, NotifyEvent event, const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len);

#endif // !__NOTIFY_H__
