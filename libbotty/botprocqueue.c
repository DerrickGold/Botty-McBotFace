#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "botprocqueue.h"

BotProcessArgs *BotProcess_makeArgs(void *data, char *responseTarget, BotProcessArgsFreeFn fn) {
  BotProcessArgs *args = calloc(1, sizeof(BotProcessArgs));
  if (!args) return NULL;

  args->data = data;
  if (responseTarget)
    args->target = strdup(responseTarget);

  args->free = fn;
  return args;
}

void BotProcess_freeArgs(BotProcessArgs *args) {
  if (!args) return;

  if (args->free)
    args->free(args->data);

  if (args->target) {
    free(args->target);
    args->target = NULL;
  }

  free(args);
}

unsigned int BotProcess_queueProcess(BotProcessQueue *procQueue, BotProcessFn fn, BotProcessArgs *args, char *cmd, char *caller) {
  BotProcess *process = calloc(1, sizeof(BotProcess));
  if (!process) {
    syslog(LOG_CRIT, "bot_queueProcess: error allocating new process");
    return 0;
  }

  process->fn = fn;
  process->arg = args;
  process->busy = 1;

  if (procQueue->head) {
    BotProcess *curProc = procQueue->head;
    while (curProc->next) {
      curProc = curProc->next;
    }
    curProc->next = process;
  }
  else {
    procQueue->head = process;
  }

  procQueue->count++;
  process->pid = (++procQueue->pidTicker);
  //catch any integer overflows, make sure pid is never 0
  if (procQueue->pidTicker == 0)
    process->pid = (++procQueue->pidTicker);

  strncpy(process->owner, caller, MAX_NICK_LEN - 1);
  snprintf(process->details, MAX_MSG_LEN, "PID: %d: %s - %s", process->pid, cmd, caller);
  syslog(LOG_DEBUG, "bot_queueProcess: %s Added new process to queue:\n %s", process->owner, process->details);
  return process->pid;
}

unsigned int BotProcess_dequeueProcess(BotProcessQueue *procQueue, BotProcess *process) {
  if (!process) return 0;

  unsigned int pid = process->pid;
  if (procQueue->head != process) {
    BotProcess *proc = procQueue->head;
    while (proc->next != process) {
      proc = proc->next;
    }
    proc->next = process->next;
  }
  else {
    procQueue->head = process->next;
  }

  if (procQueue->current == process)
    procQueue->current = process->next;

  procQueue->count--;
  //if process is dequeued while it was running, cleanup the process data
  if (process->busy >= 0) BotProcess_freeArgs(process->arg);

  syslog(LOG_DEBUG, "bot_queueProcess: Removed process:\n %s", process->details);
  free(process);
  return pid;
}

BotProcess *BotProcess_findProcessByPid(BotProcessQueue *procQueue, unsigned int pid) {
  BotProcess *process = procQueue->head;

  while (process && process->pid != pid) {
    process = process->next;
  }

  if (!process)
    syslog(LOG_WARNING, "Failed to located PID: %d", pid);
  else
    syslog(LOG_DEBUG, "Located Process:\n %s", process->details);

  return process;
}

unsigned int BotProcess_updateProcessQueue(BotProcessQueue *procQueue, void *botInfo) {
  unsigned int terminatedPid = 0;
  if (!procQueue->current)
    procQueue->current = procQueue->head;

  BotProcess *proc = procQueue->current;
  if (proc && proc->fn) {
    procQueue->curPid = procQueue->current->pid;
    if (proc->terminate || (proc->busy = proc->fn(botInfo, proc->owner, proc->arg)) < 0)
      terminatedPid = BotProcess_dequeueProcess(procQueue, proc);
    else
      procQueue->current = proc->next;
  }
  procQueue->curPid = 0;
  return terminatedPid;
}

void BotProcess_freeProcesaQueue(BotProcessQueue *procQueue) {
  while (procQueue->count > 0 && procQueue->head)
    BotProcess_dequeueProcess(procQueue, procQueue->head);
}

void BotProcess_terminate(BotProcess *process) {
  process->terminate = 1;
}
