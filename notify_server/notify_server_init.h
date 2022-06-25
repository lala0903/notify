#ifndef __NOTIFY_SERVER_INIT_H__
#define __NOTIFY_SERVER_INIT_H__

int GetServerSocket(void);
int NotifyServerInit(void);
int GetEpollFd(void);

#endif // !__NOTIFY_SERVER_INIT_H__