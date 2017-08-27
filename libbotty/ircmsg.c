#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "ircmsg.h"

IrcMsg *ircMsg_newMsg(void) {
  IrcMsg *msg = NULL;
  msg = calloc(1, sizeof(IrcMsg));
  if (!msg) {
    fprintf(stderr, "[FATAL] IrcMsg alloc error\n");
    exit(1);    
  }

  return msg;
}

void ircMsg_setChannel(IrcMsg *msg, char *channel) {
  if (!channel) {
    fprintf(stderr, "Could not set NULL channel to IrcMsg\n");
    return;
  }
  strncpy(msg->channel, channel, MAX_CHAN_LEN);
}


IrcMsg *ircMsg_irc_new(char *input, HashTable *cmdTable, HashTable *cmdAliases, BotCmd **cmd) {
  IrcMsg *msg = ircMsg_newMsg();
  char *end = input + strlen(input);
  char *tok = NULL, *tok_off = NULL;
  int i = 0;

  //first get the nick that created the message
  tok = strtok_r(input, "!", &tok_off);
  if (!tok) return msg;
  strncpy(msg->nick, tok+1, MAX_NICK_LEN);
  //skip host name
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok) return msg;

  //get action issued
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok) return msg;
  strncpy(msg->action, tok, MAX_CMD_LEN);

  //get the channel or user the message originated from
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok) return msg;
  //if the token starts with the delimiter at this point, then
  //there is no channel parameter, just a message
  if (*tok != PARAM_DELIM) {
    strncpy(msg->channel, tok, MAX_CHAN_LEN);
    if (!tok_off || tok_off + 1 >= end) return msg;
  } else
    tok_off = tok;

  //finally save the rest of the message
  strncpy(msg->msg, tok_off+1, MAX_MSG_LEN);

  //parse a given command
  if (msg->msg[0] == CMD_CHAR && cmd) {
    int argCount = MAX_BOT_ARGS;
    tok = msg->msg + 1;
    while(i < argCount) {
      tok_off = strchr(tok, BOT_ARG_DELIM);
      if (tok_off && i < argCount - 1) *tok_off = '\0';
      msg->msgTok[i] = tok;

      if (i == 0) {
        *cmd = command_get(cmdTable, msg->msgTok[0]);
        if (*cmd)  argCount = (*cmd)->args;
        else {
          CmdAlias *alias = command_alias_get(cmdAliases, msg->msgTok[0]);
          if (alias) {
            *cmd = alias->cmd;
            argCount = alias->cmd->args;
            for (i = 0; i < alias->argc; i++)
              msg->msgTok[i] = alias->args[i];

            i--;
          }
        }
      }

      if (!tok_off) break;
      tok_off++;
      tok = tok_off;
      i++;
    }
  }

  return msg;
}

IrcMsg *ircMsg_server_new(char *input) {
  IrcMsg *msg = ircMsg_newMsg();;
  int i = 0;
  char *end = input + strlen(input);
  char *tok = NULL, *tok_off = NULL;

  msg->server = 1;
  //skip the server
  tok = strtok_r(input, " ", &tok_off);
  if (!tok) return msg;

  //copy the status code
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok) return msg;
  strncpy(msg->action, tok, MAX_CMD_LEN);

  //skip the name the server issued the command to
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok || !tok_off || tok_off + 1 >= end) return msg;

  //copy the rest of the message
  strncpy(msg->msg, tok_off, MAX_MSG_LEN);

  //tokenize the parameters given by the server
  tok = msg->msg;
  tok += (*tok == PARAM_DELIM);
  while(i < MAX_PARAMETERS) {
    tok_off = strchr(tok, PARAM_DELIM);
    msg->msgTok[i] = tok;
    if (!tok_off || i >= MAX_PARAMETERS - 1) break;
    (*tok_off++) = '\0';
    tok = tok_off;
    i++;
  }

  return msg;
}


