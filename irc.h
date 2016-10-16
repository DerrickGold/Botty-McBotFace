#ifndef __IRC_H__
#define __IRC_H__

#include "globals.h"
#include "callback.h"

typedef enum {
  CONSTATE_NONE,
  CONSTATE_CONNECTED,
  CONSTATE_CONFIRM_REG,
  CONSTATE_REGISTERED,
  CONSTATE_JOINED,
  CONSTATE_LISTENING,
} ConState;


typedef struct IrcInfo {
  char host[256];
  char nick[MAX_NICK_LEN];
  char port[6];
  char ident[10];
  char realname[64];
  char master[30];
  char server[MAX_SERV_LEN];
  char channel[MAX_CHAN_LEN];
  int servfd;
  ConState state;
  int nickNum;
} IrcInfo;

extern int run(IrcInfo *info, int argc, char *argv[], int argstart);

extern int ircSend(int fd, const char *msg);

extern int botSend(IrcInfo *info, char *target, char *msg);

#endif //__IRC_H__
