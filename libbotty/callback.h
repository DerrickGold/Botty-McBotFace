#ifndef __CALLBACK_H__
#define __CALLBACK_H__

#include "ircmsg.h"

//Used in conjunction with cbfn in IrcInfo
typedef enum {
  CALLBACK_CONNECT,
  CALLBACK_JOIN,
  CALLBACK_MSG,
  CALLBACK_USRJOIN,
  CALLBACK_USRPART,
  CALLBACK_SERVERCODE,
  CALLBACK_USRNICKCHANGE,
  CALLBACK_USRINVITE,
  CALLBACK_COUNT,
} BotCallbackID;


typedef int(*Callback)(void *, IrcMsg *);

void callback_set_r(Callback collection[CALLBACK_COUNT], BotCallbackID id, Callback fn);
void callback_set(BotCallbackID id, Callback fn);


int callback_call_r(Callback collection[CALLBACK_COUNT], BotCallbackID id, void *data, IrcMsg *msg);
int callback_call(BotCallbackID id, void *data, IrcMsg *msg);


#endif //__CALLBACK_H__
