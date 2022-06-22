#ifndef __NOTIFY_CLIENT_SEND_H__
#define __NOTIFY_CLIENT_SEND_H__

#include "notify_common.h"

#define SYNC_TYPE 0
#define ASYNC_TYPE 1

typedef enum{
    ASNYC_MSG,
    SNYC_MSG,
    REG_MSG,
    ACK_MSG,
} MsgType;

typedef enum{
    NO_ACK,
    ACK_OK,
    ACK_ERR,
} MsgAskType;

struct SendMsgFrame {
    unsigned int msgType;
    unsigned int destId;
    unsigned int event;
    void *par1;
    unsigned int par1Len;
    void *par2;
    unsigned int par2Len;
    int retValue;
};

struct AsyncMsgHead {
    unsigned int destId;
    unsigned int event;
    void *par1;
    unsigned int par1Len;
    void *par2;
    unsigned int par2Len;
};

struct MsgHeadInfo {
    unsigned int seqNum;
    unsigned int sourceId;
    unsigned int msgType;
    unsigned int destId;
    unsigned int event;
    unsigned int syncType;
    MsgAskType ackType;
    unsigned int totalLen;
    unsigned int par1Len;
    unsigned int par2Len;
    unsigned int outLen;
    int retValue;
};

#define INIT_SEND_MSG_FRAME(Msg, msgType, destId, event, par1, par1Len, par2, par2Len, retValue) \
 do { \
    Msg.msgType = msgType; \
    Msg.destId = destId; \
    Msg.event = event;  \
    Msg.par1 = par1;    \
    Msg.par1Len = par1Len;  \
    Msg.par2 = par2;    \
    Msg.par2Len = par2Len;  \
    Msg.retValue = retValue;    \
 } while (0)
 
typedef int (*RegisterSyncFunc)(NotifyEvent event, const void *input, unsigned int inLen, void *output, unsigned int outLen);
typedef int (*RegisterAsyncFunc)(NotifyEvent event, const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len);

RegisterAsyncFunc GetAsyncFunc(NotifyModuleId moduleId);
RegisterAsyncFunc GetSyncFunc(NotifyModuleId moduleId);
int SendMsgToServer(struct MsgHeadInfo *head, unsigned int seqNum);
void WakeupDataWaite(void);
void DataLock(void);
void DataUnlock(void);
void ClearDestModuleIdValue(NotifyModuleId moduleId);

#endif // !__NOTIFY_CLIENT_SEND_H__