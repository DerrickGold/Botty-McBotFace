#ifndef __LIBBOTTY_IRC_PROCESSQUEUE_H__
#define __LIBBOTTY_IRC_PROCESSQUEUE_H__

#include "globals.h"

typedef int (*BotProcessArgsFreeFn)(void *);

typedef struct BotProcessArgs {
  void *data;
  char *target;
  BotProcessArgsFreeFn free;
} BotProcessArgs;

typedef int (*BotProcessFn)(void *, BotProcessArgs *);

typedef struct BotProcess {
  BotProcessFn fn;
  BotProcessArgs *arg;
  char busy;
  struct BotProcess *next;
  unsigned int pid;
  char details[MAX_MSG_LEN];
} BotProcess;

typedef struct BotProcessQueue {
  int count;
  unsigned int pidTicker;
  BotProcess *head;
  BotProcess *current;
  unsigned int curPid;
} BotProcessQueue;

BotProcessArgs *BotProcess_makeArgs(void *data, char *responseTarget, BotProcessArgsFreeFn fn);
void BotProcess_freeArgs(BotProcessArgs *args);
unsigned int BotProcess_queueProcess(BotProcessQueue *procQueue, BotProcessFn fn, BotProcessArgs *args, char *cmd, char *caller);
void BotProcess_dequeueProcess(BotProcessQueue *procQueue, BotProcess *process);
BotProcess *BotProcess_findProcessByPid(BotProcessQueue *procQueue, unsigned int pid);
void BotProcess_updateProcessQueue(BotProcessQueue *procQueue, void *botInfo);
void BotProcess_freeProcesaQueue(BotProcessQueue *procQueue);

#endif //__LIBBOTTY_IRC_PROCESSQUEUE_H__
