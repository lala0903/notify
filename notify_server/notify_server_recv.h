#ifndef __NOTIFY_SERVER_RECV_H__
#define __NOTIFY_SERVER_RECV_H__

int CreateRecvThread(void);
void RemoveClientListen(int clientFd);

#endif // !__NOTIFY_SERVER_RECV_H__