#include "botmsgqueues.h"

static long long calculateNextMsgTime(char throttled) {

  long long curTime = botty_currentTimestamp();
  if (throttled) curTime += THROTTLE_WAIT_SEC * ONE_SEC_IN_MS;
  else curTime += (ONE_SEC_IN_MS/MSG_PER_SECOND_LIM);
  return curTime;
}

static void initMsgQueue(BotSendMessageQueue *queue) {
  queue->nextSendTimeMS = botty_currentTimestamp();
}


int BotMsgQueue_init(HashTable **msgQueue) {
  if (!msgQueue) {
    syslog(LOG_CRIT, "%s: Null hashtable pointer provided.", __FUNCTION__);
    return -1;
  }

  *msgQueue = HashTable_init(COMMAND_HASH_SIZE);
  if (!*msgQueue) {
    syslog(LOG_CRIT, "%s: Error allocating msgQueue hash for bot", __FUNCTION__);
    return -1;
  }

  return 0;
}


BotQueuedMessage *BotQueuedMsg_newMsg(char *msg, char *responseTarget, size_t len, unsigned int createdByPid) {
  BotQueuedMessage *newMsg = calloc(1, sizeof(BotQueuedMessage));
  if (!newMsg) {
    syslog(LOG_CRIT, "newQueueMsg: Error allocating new message for:\n%s to %s", msg, responseTarget);
    return NULL;
  }
  strncpy(newMsg->msg, msg, MAX_MSG_LEN);
  strncpy(newMsg->channel, responseTarget, MAX_CHAN_LEN);
  newMsg->status = QUEUED_STATE_INIT;
  newMsg->len = len;
  newMsg->createdByPid = createdByPid;
  return newMsg;
}

static void freeQueueMsg(BotQueuedMessage *msg) {
  free(msg);
}

static BotQueuedMessage *peekQueueMsg(BotSendMessageQueue *queue) {
  if (!queue || !queue->start)
    return NULL;

  return queue->start;
}

static BotQueuedMessage *popQueueMsg(BotSendMessageQueue *queue) {
  if (!queue || !queue->start)
    return NULL;

  BotQueuedMessage *poppedMsg = queue->start;
  queue->start = poppedMsg->next;
  if (queue->end == poppedMsg) queue->end = NULL;
  queue->count--;
  syslog(LOG_INFO, "%d queued messages", queue->count);
  return poppedMsg;
}

static void pushQueueMsg(BotSendMessageQueue *queue, BotQueuedMessage *msg) {
  if (!queue || !msg)
    return;

  msg->next = queue->start;
  if (!queue->start)
    queue->end = msg;
  queue->start = msg;
  queue->count++;
  syslog(LOG_INFO, "%d queued messages", queue->count);
}

static void enqueueMsg(BotSendMessageQueue *queue, BotQueuedMessage *msg) {
  if (!queue || !msg)
    return;

  queue->count++;
  if (!queue->end && !queue->start) {
    queue->start = msg;
    queue->end = msg;
    return;
  }
  queue->end->next = msg;
  queue->end = msg;
}

void BotMsgQueue_enqueueTargetMsg(HashTable *msgQueues, char *target, BotQueuedMessage *msg) {
  HashEntry *targetQueue = HashTable_find(msgQueues, target);
  if (!targetQueue) {
    syslog(LOG_DEBUG, "Creating new message queue hash for: %s", target);
    BotSendMessageQueue *newQueue = calloc(1, sizeof(BotSendMessageQueue));
    if (!newQueue) {
      syslog(LOG_CRIT, "Error creating message queue for target: %s", target);
      return;
    }

    char *queueKey = strdup(target);
    if (!queueKey) {
      syslog(LOG_CRIT, "Error creating queue name: %s", target);
      return;
    }

    initMsgQueue(newQueue);
    targetQueue = HashEntry_create(queueKey, (void*)newQueue);
    syslog(LOG_INFO, "%s: Adding message queue for %s to hash", __FUNCTION__, target);
    HashTable_add(msgQueues, targetQueue);
  }

  enqueueMsg((BotSendMessageQueue *)targetQueue->data, msg);
}

void BotMsgQueue_setThrottle(HashTable *msgQueues, char *target) {
  HashEntry *targetQueueEntry = HashTable_find(msgQueues, target);
  if (!targetQueueEntry) return;

  BotSendMessageQueue *msgQueue = (BotSendMessageQueue *)targetQueueEntry->data;
  msgQueue->throttled++;
}

void BotMsgQueue_processQueue(SSLConInfo *conInfo, BotSendMessageQueue *queue) {
  //BotSendMessageQueue *queue = &bot->msgQueue;
  TimeStamp_t currentTime = botty_currentTimestamp();
  TimeStamp_t timeDiff = currentTime - queue->nextSendTimeMS;

  if (timeDiff < 0) return;

  if (queue->isThrottled) {
    queue->lastThrottled = queue->throttled;
    syslog(LOG_NOTICE, "resetting throttle limit");
  }
  queue->isThrottled = (queue->throttled != queue->lastThrottled);

  int ret = 0;
  if (!connection_client_poll(conInfo, POLLOUT, &ret)) {
    syslog(LOG_DEBUG, "processMsgQueue: socket not ready for output");
    return;
  }

  BotQueuedMessage *msg = peekQueueMsg(queue);
  if (!msg) return;

  switch (msg->status) {
    case QUEUED_STATE_INIT: {
      msg->status = QUEUED_STATE_SENT;
      syslog(LOG_DEBUG, "SENDING (%d bytes): %s", (int)msg->len, msg->msg);
      queue->writeStatus = connection_client_send(conInfo, msg->msg, msg->len);
      queue->nextSendTimeMS = calculateNextMsgTime(0);
    } break;
    case QUEUED_STATE_SENT: {
      if (queue->isThrottled) {
        syslog(LOG_WARNING, "Throttled, will retry sending %s", msg->msg);
        queue->nextSendTimeMS = calculateNextMsgTime(1);
        msg->status = QUEUED_STATE_INIT;
      } else {
        syslog(LOG_DEBUG, "Successfully sent: %d bytes", (int)msg->len);
        msg = popQueueMsg(queue);
        freeQueueMsg(msg);
        queue->nextSendTimeMS = calculateNextMsgTime(0);
      }
    } break;
  }
}

static int cleanQueue(HashEntry *entry, void *data) {
  if (entry->data) {
    BotSendMessageQueue *queue = (BotSendMessageQueue *)entry->data;
    syslog(LOG_INFO, "Cleaning message queue: %s: %d", entry->key, queue->count);
    while (queue->count > 0) {
      BotQueuedMessage *msg = popQueueMsg(queue);
      if (msg) freeQueueMsg(msg);
    }
    free(queue);
    free(entry->key);
  }
  return 0;
}

int BotMsgQueue_rmPidMsg(HashTable *msgQueues, char *target, unsigned int pid) {

  int removed = 0;
  HashEntry *targetQueue = HashTable_find(msgQueues, target);
  if (!targetQueue)
    return 0;

  BotSendMessageQueue *queue = (BotSendMessageQueue *)targetQueue->data;
  if (!queue || queue->count == 0)
    return 0;

  BotQueuedMessage *prevMessage = queue->start;
  BotQueuedMessage *curMessage = queue->start;

  while (curMessage) {
    if (curMessage->createdByPid == pid) {
      if (curMessage == queue->start) {
        BotQueuedMessage *first = popQueueMsg(queue);
        freeQueueMsg(first);
        curMessage = queue->start;
      } else {
        prevMessage->next = curMessage->next;
        freeQueueMsg(curMessage);
        curMessage = prevMessage->next;
        queue->count--;
      }
      removed++;
      syslog(LOG_INFO, "Removed message from queue: %s:%d. %d Message(s) remain.", target, pid, queue->count);
    }
    else {
      prevMessage = curMessage;
      curMessage = curMessage->next;
    }
  }
  return removed;
}

void BotMsgQueue_cleanQueues(HashTable **msgQueues) {
  HashTable_forEach(*msgQueues, NULL, &cleanQueue);
  HashTable_destroy(*msgQueues);
  *msgQueues = NULL;
}

