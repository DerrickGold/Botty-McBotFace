#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "botinputqueue.h"


BotQueuedInput *BotInput_newQueuedInput(char *input) {
  if (!input || strlen(input) == 0)
    return NULL;

  syslog(LOG_INFO, "Creating new queued input");

  BotQueuedInput *queuedInput = calloc(1, sizeof(BotQueuedInput));
  if (!queuedInput) {
    syslog(LOG_CRIT, "Failed to allocate new queued input obj for input: %s", input);
    return NULL;
  }

  syslog(LOG_DEBUG, "copying input to queued object: %s", input);
  strncpy(queuedInput->msg, input, MAX_MSG_LEN);
  queuedInput->next = NULL;
  return queuedInput;
}

void BotInput_freeQueuedInput(BotQueuedInput *qInput) {
  if (!qInput) return;

  free(qInput);
}


int BotInputQueue_len(BotInputQueue *inputQueue) {
  return inputQueue->count;
}

void BotInputQueue_enqueueInput(BotInputQueue *inputQueue, char *input) {

  if (!inputQueue || !input || strlen(input) == 0)
    return;


  BotQueuedInput *newInput = BotInput_newQueuedInput(input);
  if (!newInput) {
   	syslog(LOG_CRIT, "Failed to allocate new queued input object: msg: %s", input);
    return;
  }

  if (!inputQueue->head) {
    inputQueue->head = newInput;
    inputQueue->end = newInput;
  }
  else {
    inputQueue->end->next = newInput;
    inputQueue->end = newInput;
  }
  inputQueue->count++;
}


BotQueuedInput *BotInputQueue_dequeueInput(BotInputQueue *inputQueue) {
  if (!inputQueue)
    return NULL;

  BotQueuedInput *nextInput = inputQueue->head;
  inputQueue->head = nextInput->next;
  if (nextInput == inputQueue->end)
    inputQueue->end = NULL;

  inputQueue->count--;
  return nextInput;
}

void BotInputQueue_initQueue(BotInputQueue *inputQueue) {
  inputQueue->head = NULL;
  inputQueue->end = NULL;
  inputQueue->count = 0;
}

void BotInputQueue_clearQueue(BotInputQueue *inputQueue) {
  while (BotInputQueue_len(inputQueue)) {
    BotQueuedInput *input = BotInputQueue_dequeueInput(inputQueue);
    BotInput_freeQueuedInput(input);
  }
}


void BotInputQueue_pushInput(BotInputQueue *inputQueue, char *input) {
  syslog(LOG_NOTICE, "Pushing message into input queue: msg: %s", input);
  if (!inputQueue || !input || strlen(input) == 0)
    return;

  BotQueuedInput *newInput = BotInput_newQueuedInput(input);
  if (!newInput) {
    syslog(LOG_CRIT, "Failed to allocate new queued input object: msg: %s", input);
    return;
  }

  BotQueuedInput *oldHead = inputQueue->head;
  inputQueue->head = newInput;
  newInput->next = oldHead;
  inputQueue->count++;
  syslog(LOG_INFO, "%d queued messages", inputQueue->count);
}

void BotInput_spoofUserInput(BotInputQueue *inputQueue, char *user, char *srcChannel, char *msg) {
  char spoofMsg[MAX_MSG_LEN];
  snprintf(spoofMsg, MAX_MSG_LEN - 1, ":%s!%s PRIVMSG %s :%s", user, INPUT_SPOOFED_HOSTNAME, srcChannel, msg);
  syslog(LOG_DEBUG, "Spoofing user bot input: %s", spoofMsg);
  BotInputQueue_enqueueInput(inputQueue, spoofMsg);
}
