#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "botinputqueue.h"


BotQueuedInput *BotInput_newQueuedInput(char *input) {
  if (!input || strlen(input) == 0)
    return NULL;

  fprintf(stderr, "Creating new queued input\n");
  
  BotQueuedInput *queuedInput = calloc(1, sizeof(BotQueuedInput));
  if (!queuedInput) {
    fprintf(stderr, "Failed to allocate new queued input obj for input: %s\n", input);
    return NULL;
  }

  fprintf(stderr, "copying message to queued object\n");
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
    fprintf(stderr, "Failed to queue new input\n");
    return;
  }

  if (!inputQueue->head) {
    fprintf(stderr, "New queued input is head!\n");
    inputQueue->head = newInput;
    inputQueue->end = newInput;
  }
  else {
    fprintf(stderr, "Adding new queued input to end of queue\n");
    inputQueue->end->next = newInput;
    inputQueue->end = newInput;
  }
  inputQueue->count++;
}


BotQueuedInput *BotInputQueue_dequeueInput(BotInputQueue *inputQueue) {
  if (!inputQueue)
    return NULL;

  fprintf(stderr, "Getting next queued message\n");
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


