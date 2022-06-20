#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

int InitThreadPool(int maxPoolSize);
void DestroyThreadPool(void);

int AddTaskInThreadPool(void (*func)(void *arg), void *arg);

#endif // !__THREADPOOL_H__