#ifndef __NOTIFY_CLIENT_SEND_H__
#define __NOTIFY_CLIENT_SEND_H__

#include "notify_common.h"

typedef enum{
    REG_MSG,
    ACK_MSG,
    ACK_ERR,
} MsgType;

typedef enum{
    
} MsgAskType;

struct SendMsgFrame {
    unsigned int msgType;
    unsigned int destModuleId;
    unsigned int event;
    void *par1;
    unsigned int par1Len;
    void *par2;
    unsigned int par2Len;
    int retValue;
};

struct MsgHeadInfo {
    unsigned int seqNum;
    unsigned int sourceId;
    unsigned int msgTypeValue;
    unsigned int destId;
    unsigned int event;
    bool syncType;
    MsgAskType ackType;
    unsigned int totalLen;
    unsigned int par1Len;
    unsigned int par2Len;
    unsigned int outLen;
    int retValue;
};

typedef int (*RegisterSyncFunc)(NotifyEvent event, const void *input, int inLen, void *output, int outLen);
typedef int (*RegisterAsyncFunc)(NotifyEvent event, const void *par1, int par1Len, const void *par2, int par2Len);

int RegisterNotifyFunction(NotifyModuleId moduleId, RegisterAsyncFunc asyncFunc, RegisterSyncFunc syncFunc);
int UnregisterNotifyFunction(NotifyModuleId moduleId);
int SendNotify(NotifyModuleId moduleId, NotifyEvent event, const void *input, int inLen, void *output, int outLen);
int PostNotify(NotifyModuleId moduleId, NotifyEvent event, const void *par1, int par1Len, const void *par2, int par2Len);

#endif // !__NOTIFY_CLIENT_SEND_H__