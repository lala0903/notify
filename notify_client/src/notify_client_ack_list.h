#ifndef __NOTIFY_CLIENT_ACK_LIST_H__
#define __NOTIFY_CLIENT_ACK_LIST_H__

int NotifyAckLisitInit(void);
void NotifyAckLisitDestroy(void);
struct NotifyAckNode *CreateAckNote(void);

struct NotifyAckNode {
    unsigned int seqNum;
    int retValue;
    void *buff;
    unsigned int len;
    struct NotifyAckNode *next;
};

#endif // !__NOTIFY_CLIENT_INIT_H__