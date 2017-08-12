#ifndef __IRCMSG_H__
#define __IRCMSG_H__

#include "globals.h"
#include "commands.h"

//easy structure for reading details of an irc message
typedef struct IrcMsg {
  char server;
  char nick[MAX_NICK_LEN];
  char action[MAX_CMD_LEN];
  char channel[MAX_CHAN_LEN];
  char msg[MAX_MSG_LEN];
  char *msgTok[MAX_PARAMETERS];
} IrcMsg;

IrcMsg *ircMsg_irc_new(char *input, HashTable *cmdTable, HashTable *cmdAliases, BotCmd **cmd);

IrcMsg *ircMsg_server_new(char *input);

#endif
