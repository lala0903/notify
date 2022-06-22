#ifndef __NOTIFY_CLIENT_ACK_LIST_H__
#define __NOTIFY_CLIENT_ACK_LIST_H__

struct NotifyAckNode {
    unsigned int seqNum;
    int retValue;
    void *buff;
    unsigned int len;
    struct NotifyAckNode *next;
};

int NotifyAckLisitInit(void);
void NotifyAckLisitDestroy(void);
struct NotifyAckNode *CreateAckNote(unsigned int seqNum, void *buff, unsigned int len, int retValue);
int IsNodeExistAckList(unsigned int seqNum);
void RemoveNodeFromAckList(unsigned int seqNum);
int InsertNodeInAckList(struct NotifyAckNode *node);
void GetDataFromeAckList(unsigned int seqNum, void *buff, unsigned int len, int *retValue);
#endif // !__NOTIFY_CLIENT_INIT_H__