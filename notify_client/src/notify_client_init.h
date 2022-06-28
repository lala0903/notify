
#ifndef __NOTIFY_CLIENT_INIT_H__
#define __NOTIFY_CLIENT_INIT_H__

#include <stdbool.h>

int GetClientSocket(void);
int ConnectNotifyServer(void);
void CloseClientSocket(void);
bool IsNOtifyInit(void);

#endif // !__NOTIFY_CLIENT_INIT_H__
