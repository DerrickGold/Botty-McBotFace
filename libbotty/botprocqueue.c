#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "botprocqueue.h"

BotProcessArgs *BotProcess_makeArgs(void *data, char *responseTarget, BotProcessArgsFreeFn fn) {
  BotProcessArgs *args = calloc(1, sizeof(BotProcessArgs));
  if (!args) return NULL;

  args->data = data;
  if (responseTarget) {
		size_t responseTargetLen = strlen(responseTarget);
  	args->target = calloc(1, responseTargetLen + 1);
  	if (!args->target) return NULL;
  	strncpy(args->target, responseTarget, responseTargetLen);
  }
  args->free = fn;
  return args;
}

void BotProcess_freeArgs(BotProcessArgs *args) {
	if (!args) return;

	if (args->free) args->free(args->data);
	if (args->target) {
		free(args->target);
		args->target = NULL;
	}
	free(args);
}


void BotProcess_queueProcess(BotProcessQueue *procQueue, BotProcessFn fn, BotProcessArgs *args, char *cmd, char *caller) {
	BotProcess *process = calloc(1, sizeof(BotProcess));
	if (!process) {
		fprintf(stderr, "bot_queueProcess: error allocating new process\n");
		return;
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
	snprintf(process->details, MAX_MSG_LEN, "PID: %d: %s - %s", process->pid, cmd, caller);
	fprintf(stderr, "bot_queueProcess: Added new process to queue:\n %s\n", process->details);
}

void BotProcess_dequeueProcess(BotProcessQueue *procQueue, BotProcess *process) {
	if (!process) return;

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

	fprintf(stderr, "bot_queueProcess: Removed process:\n %s\n", process->details);
	free(process);
}

BotProcess *BotProcess_findProcessByPid(BotProcessQueue *procQueue, unsigned int pid) {
	BotProcess *process = procQueue->head;

	while (process && process->pid != pid) {
		process = process->next;
	}

	if (!process)
		fprintf(stderr, "Failed to located PID: %d\n", pid);
	else
		fprintf(stderr, "Located Process:\n %s\n", process->details);

	return process;
}

void BotProcess_updateProcessQueue(BotProcessQueue *procQueue, void *botInfo) {
	if (!procQueue->current)
		procQueue->current = procQueue->head;

	BotProcess *proc = procQueue->current;
	if (proc && proc->fn) {
  	if ((proc->busy = proc->fn(botInfo, proc->arg)) < 0)
      BotProcess_dequeueProcess(procQueue, proc);
    else
    	procQueue->current = proc->next;
  }
}

void BotProcess_freeProcesaQueue(BotProcessQueue *procQueue) {
	while (procQueue->count > 0 && procQueue->head)
		BotProcess_dequeueProcess(procQueue, procQueue->head);
}
