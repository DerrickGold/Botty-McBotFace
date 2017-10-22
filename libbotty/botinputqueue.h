#ifndef __LIBBOTTY_IRC_INPUTQUEUE_H__
#define __LIBBOTTY_IRC_INPUTQUEUE_H__

#include "globals.h"

typedef struct BotQueuedInput {
  char msg[MAX_MSG_LEN];
  struct BotQueuedInput *next;
} BotQueuedInput;

typedef struct BotInputQueue {
  BotQueuedInput *head;
  BotQueuedInput *end;
  int count;
} BotInputQueue;

BotQueuedInput *BotInput_newQueuedInput(char *input);
void BotInput_freeQueuedInput(BotQueuedInput *qInput);
int BotInputQueue_len(BotInputQueue *inputQueue);
void BotInputQueue_enqueueInput(BotInputQueue *inputQueue, char *input);
BotQueuedInput *BotInputQueue_dequeueInput(BotInputQueue *inputQueue);
void BotInputQueue_initQueue(BotInputQueue *inputQueue);
void BotInputQueue_clearQueue(BotInputQueue *inputQueue);
void BotInputQueue_pushInput(BotInputQueue *inputQueue, char *input);
void BotInput_spoofUserInput(BotInputQueue *inputQueue, char *user, char *srcChannel, char *msg);
#endif
