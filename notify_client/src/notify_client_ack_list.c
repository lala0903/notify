
#include "notify_client_ack_list.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

struct NotifyAckList {
    struct NotifyAckNode *head;
    struct NotifyAckNode *rail;
    unsigned int cnt;
};

static struct NotifyAckList *g_ackList = NULL;

int NotifyAckLisitInit(void)
{
    g_ackList = (struct NotifyAckList *)malloc(sizeof(struct NotifyAckNode));
    if (g_ackList == NULL) {
        return -1;
    }
    g_ackList->head = NULL;
    g_ackList->rail = NULL;
    g_ackList->cnt = 0;
    return 0;
}

void NotifyAckLisitDestroy(void)
{
    if (g_ackList == NULL) {
        return;
    }
    struct NotifyAckNode *temp = g_ackList->head;
    while (temp != NULL) {

    }
    g_ackList->head = NULL;
    g_ackList->rail = NULL;
    g_ackList->cnt = 0;
    return 0;
}

struct NotifyAckNode *CreateAckNote(void)
{
    struct NotifyAckNode *temp = (struct NotifyAckNode *)malloc(sizeof(struct NotifyAckNode));
    if (temp == NULL) {
        return NULL;
    }
    temp->next = NULL;
    temp->buff = NULL;
    temp->seqNum = 0;
    temp->retValue = 0;
    temp->len = 0;
    return temp;
}

int 