#ifndef __NOTIFY_CLIENT_SEND_H__
#define __NOTIFY_CLIENT_SEND_H__

#include "notify_common.h"
#include "notify.h"

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

#define INIT_SEND_MSG_FRAME(MsgFrame, type, destModuleId, eventValue, para1, para1Len, para2, para2Len, value) \
 do { \
    MsgFrame.msgType = type; \
    MsgFrame.destId = destModuleId; \
    MsgFrame.event = eventValue;  \
    MsgFrame.par1 = (void *)para1;    \
    MsgFrame.par1Len = para1Len;  \
    MsgFrame.par2 = (void *)para2;    \
    MsgFrame.par2Len = para2Len;  \
    MsgFrame.retValue = value;    \
 } while (0)

RegisterAsyncFunc GetAsyncFunc(NotifyModuleId moduleId);
RegisterSyncFunc GetSyncFunc(NotifyModuleId moduleId);
int SendMsgToServer(const struct SendMsgFrame *msg, unsigned int seqNum);
void WakeupDataWaite(void);
void DataLock(void);
void DataUnlock(void);
void ClearDestModuleIdValue(NotifyModuleId moduleId);
int ReplyMessage(struct MsgHeadInfo *head);
void UnregisterNotifyFunction(void);
int PrintHeadInfo(struct MsgHeadInfo *head, char *buff);
#endif // !__NOTIFY_CLIENT_SEND_H__