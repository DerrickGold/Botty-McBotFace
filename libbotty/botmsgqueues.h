#ifndef __LIBBOTTY_IRC_MSGQUEUE_H__
#define __LIBBOTTY_IRC_MSGQUEUE_H__

#include "globals.h"
#include "connection.h"
#include "hash.h"

typedef enum {
  QUEUED_STATE_INIT,
  QUEUED_STATE_SENT,
} BotQueuedMessageState;


typedef struct BotQueuedMessage {
  char msg[MAX_MSG_LEN];
  char channel[MAX_CHAN_LEN];
  size_t len;
  unsigned int createdByPid;
  BotQueuedMessageState status;
  struct BotQueuedMessage *next;
} BotQueuedMessage;

typedef struct BotSendMessageQueue {
  BotQueuedMessage *start;
  BotQueuedMessage *end;
  int count;
  TimeStamp_t nextSendTimeMS;
  int writeStatus;
  char isThrottled;
  int throttled, lastThrottled;
} BotSendMessageQueue;

BotQueuedMessage *BotQueuedMsg_newMsg(char *msg, char *responseTarget, size_t len, unsigned int createdByPid);
void BotMsgQueue_enqueueTargetMsg(HashTable *msgQueues, char *target, BotQueuedMessage *msg);
void BotMsgQueue_processQueue(SSLConInfo *conInfo, BotSendMessageQueue *queue);
void BotMsgQueue_setThrottle(HashTable *msgQueues, char *target);
void BotMsgQueue_cleanQueues(HashTable *msgQueues);
int BotMsgQueue_rmPidMsg(HashTable *msgQueues, char *target, unsigned int pid);

#endif //__LIBBOTTY_IRC_MSGQUEUE_H__
