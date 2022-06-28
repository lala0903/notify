#ifndef __NOTIFY_CLIENT_RECV_H__
#define __NOTIFY_CLIENT_RECV_H__

#define IS_MODULEID_INVAILD(moduleId) (moduleId < MODULE_NOTIFY_SERVER || moduleId >= MODULE_ID_MAX)

int CreateRecvThread(void);
void DestoryRecvThread(void);

#endif // !__NOTIFY_CLIENT_RECV_H__