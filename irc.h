#ifndef __IRC_H__
#define __IRC_H__

#include <stdarg.h>
#include "globals.h"
#include "commands.h"

typedef enum {
  CONSTATE_NONE,
  CONSTATE_CONNECTED,
  CONSTATE_REGISTERED,
  CONSTATE_JOINED,
  CONSTATE_LISTENING,
} ConState;


typedef struct BotInfo {
  //user config values
  char host[256];
  char nick[NICK_ATTEMPTS][MAX_NICK_LEN];
  char port[6];
  char ident[10];
  char realname[64];
  char master[30];
  char server[MAX_SERV_LEN];
  char channel[MAX_CHAN_LEN];

  //connection state info
  struct addrinfo *res;
  struct pollfd servfds;
  char recvbuf[MAX_MSG_LEN];
  char *line, *line_off;
  ConState state;
  int nickAttempt;

  BotCmd *commands;
} BotInfo;

extern void bot_addcommand(BotInfo *info, char *cmd, int flags, int args, CommandFn fn);

extern int bot_connect(BotInfo *info, int argc, char *argv[], int argstart);

extern void bot_cleanup(BotInfo *info);

extern int bot_run(BotInfo *info);

extern int ircSend(int fd, const char *msg);

extern int botSend(BotInfo *info, char *target, char *msg, ...);

extern int ctcpSend(BotInfo *info, char *target, char *command, char *msg, ...);
#endif //__IRC_H__
