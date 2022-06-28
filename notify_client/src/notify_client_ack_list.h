#ifndef __NOTIFY_CLIENT_ACK_LIST_H__
#define __NOTIFY_CLIENT_ACK_LIST_H__

#include <stdbool.h>
#include "notify.h"

struct NotifyAckNode {
    unsigned int seqNum;
    struct MsgHeadInfo *head;
    struct NotifyAckNode *next;
};

int NotifyAckLisitInit(void);
void NotifyAckLisitDestroy(void);
struct NotifyAckNode *CreateAckNote(struct MsgHeadInfo *head);
bool IsSeqNumExistInAckList(unsigned int seqNum);
int InsertNodeInAckList(struct NotifyAckNode *node);
struct NotifyAckNode *RemoveNodeFromAckList(unsigned int seqNum);
void ReleaseAckNode(struct NotifyAckNode *node);

#endif // !__NOTIFY_CLIENT_INIT_H__