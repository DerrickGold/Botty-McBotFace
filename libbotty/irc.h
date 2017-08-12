#ifndef __IRC_H__
#define __IRC_H__

#include <stdarg.h>
#include "globals.h"
#include "commands.h"
#include "callback.h"
#include "connection.h"
#include "botprocqueue.h"

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

typedef struct BotInfo {
  //user config values
  int id;
  IrcInfo *info;
  char host[256];
  char nick[NICK_ATTEMPTS][MAX_NICK_LEN];
  char ident[10];
  char realname[64];
  char master[30];
  char useSSL;
  char joined;

  //connection state info
  char recvbuf[MAX_MSG_LEN];
  char *line, *line_off;
  ConState state;
  int nickAttempt;

  Callback cb[CALLBACK_COUNT];
  HashTable *commands;
  NickList *names;

  BotProcessQueue procQueue;

  SSLConInfo conInfo;
  TimeStamp_t startTime;

  HashTable *msgQueues;
  HashTable *cmdAliases;
  //some pointer the user can use
  void *data;
} BotInfo;


int bot_irc_init(void);

void bot_irc_cleanup(void);

int bot_irc_send(BotInfo *bot, char *msg);

int bot_init(BotInfo *bot, int argc, char *argv[], int argstart);

int bot_connect(BotInfo *info);

char *bot_getNick(BotInfo *bot);

void bot_cleanup(BotInfo *info);

void bot_setCallback(BotInfo *bot, BotCallbackID id, Callback fn);

void bot_addcommand(BotInfo *info, char *cmd, int flags, int args, CommandFn fn);

int bot_run(BotInfo *info);

int bot_send(BotInfo *info, char *target, char *action, char *ctcp, char *msg, ...);

int bot_ctcp_send(BotInfo *info, char *target, char *command, char *msg, ...);

void bot_regName(BotInfo *bot, char *nick);

void bot_rmName(BotInfo *bot, char *nick);

void bot_purgeNames(BotInfo *bot);

void bot_foreachName(BotInfo *bot, void *d, void (*fn) (NickList *nick, void *data));

int bot_isThrottled(BotInfo *bot);

void bot_runProcess(BotInfo *bot, BotProcessFn fn, BotProcessArgs *args, char *cmd, char *caller);

int bot_registerAlias(BotInfo *bot, char *alias, char *cmd);

#endif //__IRC_H__
