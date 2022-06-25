#ifndef __NOTIFY_SERVER_SEND_H__
#define __NOTIFY_SERVER_SEND_H__

#include "notify.h"

int InitMessageList(void);
int AddMessageInList(struct MsgHeadInfo *head, int fd);

#endif // !__NOTIFY_SERVER_SEND_H__