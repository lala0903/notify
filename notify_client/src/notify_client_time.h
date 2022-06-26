#ifndef __NOTIFY_CLIENT_TIME_H__
#define __NOTIFY_CLIENT_TIME_H__

int IsSeqNumTimeOut(unsigned int seqNum);
int AddSeqNumInTimeoutList(unsigned int seqNum);
int ReomveSeqNumFromTimeoutList(unsigned int seqNum);
int CreateTimeOutListenerThread(void);
void DestoryTimeOutListenThread(void);

#endif // !__NOTIFY_CLIENT_INIT_H__