

#ifndef __NOTIFY_H__
#define __NOTIFY_H__

#include "notify_common.h"

int NotifyInit(void);
void NotifyDestroy(void);
int RegisterNotifyFunction(NotifyModuleId moduleId, RegisterAsyncFunc asyncFunc, RegisterSyncFunc syncFunc);
int UnregisterNotifyFunction(NotifyModuleId moduleId);
int SendNotify(NotifyModuleId moduleId, NotifyEvent event, const void *input, unsigned int inLen, void *output, unsigned int outLen);
int PostNotify(NotifyModuleId moduleId, NotifyEvent event, const void *par1, unsigned int par1Len, const void *par2, unsigned int par2Len);

#endif // !__NOTIFY_H__
