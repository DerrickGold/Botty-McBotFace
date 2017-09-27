#include <stdio.h>
#include <stdlib.h>
#include "callback.h"

static Callback cb[CALLBACK_COUNT];
/*
 * Setter and callers for callbacks assigned to a given IrcInfo instance.
 */
void callback_set_r(Callback collection[CALLBACK_COUNT], BotCallbackID id, Callback fn) {
  if (id > CALLBACK_COUNT) {
    syslog(LOG_ERR, "setcb: Callback ID %d does not exist", (int)id);
    return;
  }
  collection[id] = fn;
}

void callback_set(BotCallbackID id, Callback fn) {
  callback_set_r(cb, id, fn);
}


int callback_call_r(Callback collection[CALLBACK_COUNT], BotCallbackID id, void *data, IrcMsg *msg) {
  if (id > CALLBACK_COUNT) {
    syslog(LOG_ERR, "callcb: Callback ID %d does not exist", (int)id);
    return -1;
  }

  if (collection[id]) return collection[id](data, msg);
  return -1;
}

int callback_call(BotCallbackID id, void *data, IrcMsg *msg) {
  return callback_call_r(cb, id, data, msg);
}
