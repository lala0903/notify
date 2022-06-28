#ifndef __NOTIFY_CLIENT_TIME_H__
#define __NOTIFY_CLIENT_TIME_H__

#include <stdbool.h>

bool IsSeqNumTimeOut(unsigned int seqNum);
int AddSeqNumInTimeoutList(unsigned int seqNum);
void ReomveSeqNumFromTimeoutList(unsigned int seqNum);
int CreateTimeOutListenerThread(void);
void DestoryTimeOutListenerThread(void);
bool IsSeqEXistInTimerlist(unsigned int seqNum);

#endif // !__NOTIFY_CLIENT_INIT_H__