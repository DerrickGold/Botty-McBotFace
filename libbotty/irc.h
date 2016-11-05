#ifndef __IRC_H__
#define __IRC_H__

#include <stdarg.h>
#include "globals.h"
#include "commands.h"
#include "callback.h"

typedef enum {
  CONSTATE_NONE,
  CONSTATE_CONNECTED,
  CONSTATE_REGISTERED,
  CONSTATE_JOINED,
  CONSTATE_LISTENING,
} ConState;

typedef struct NickList {
  char nick[MAX_NICK_LEN];
  struct NickList *next;
} NickList;

typedef struct IrcInfo {
  char port[6];
  char server[MAX_SERV_LEN];
  char channel[MAX_CHAN_LEN];
} IrcInfo;


typedef int (*BotProcessFn)(void *, void *);

typedef struct BotProcess {
  BotProcessFn fn;
  void *arg;
  char busy;
} BotProcess;


typedef struct BotInfo {
  //user config values
  int id;
  IrcInfo *info;
  char host[256];
  char nick[NICK_ATTEMPTS][MAX_NICK_LEN];
  char ident[10];
  char realname[64];
  char master[30];
  
  //connection state info
  struct addrinfo *res;
  struct pollfd servfds;
  char recvbuf[MAX_MSG_LEN];
  char *line, *line_off;
  ConState state;
  int nickAttempt;

  Callback cb[CALLBACK_COUNT];
  HashTable *commands;
  NickList *names;

  BotProcess process;
  //some pointer the user can use
  void *data;
} BotInfo;


extern int irc_init(void);

extern void irc_cleanup(void);

extern int bot_init(BotInfo *bot, int argc, char *argv[], int argstart);

extern int bot_connect(BotInfo *info);

extern char *bot_getNick(BotInfo *bot);

extern void bot_cleanup(BotInfo *info);

extern void bot_setProcess(BotInfo *bot, BotProcessFn fn, void *args);

extern void bot_clearProcess(BotInfo *bot);

extern void bot_runProcess(BotInfo *bot);

extern int bot_isProcessing(BotInfo *bot);

extern void bot_setCallback(BotInfo *bot, BotCallbackID id, Callback fn);

extern void bot_addcommand(BotInfo *info, char *cmd, int flags, int args, CommandFn fn);

extern int bot_run(BotInfo *info);

extern int ircSend(int fd, char *msg);

extern int botSend(BotInfo *info, char *target, char *action, char *ctcp, char *msg, ...);

extern int ctcpSend(BotInfo *info, char *target, char *command, char *msg, ...);

extern void bot_regName(BotInfo *bot, char *nick);

extern void bot_rmName(BotInfo *bot, char *nick);

extern void bot_purgeNames(BotInfo *bot);

extern void bot_foreachName(BotInfo *bot, void *d, void (*fn) (NickList *nick, void *data));

#endif //__IRC_H__
