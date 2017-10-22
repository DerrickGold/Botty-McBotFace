#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "ircmsg.h"

IrcMsg *ircMsg_newMsg(void) {
  IrcMsg *msg = NULL;
  msg = calloc(1, sizeof(IrcMsg));
  if (!msg) {
    syslog(LOG_CRIT, "IrcMsg alloc error");
    exit(1);
  }

  return msg;
}

void ircMsg_setChannel(IrcMsg *msg, char *channel) {
  if (!channel) {
    syslog(LOG_CRIT, "Could not set NULL channel to IrcMsg");
    return;
  }
  strncpy(msg->channel, channel, MAX_CHAN_LEN);
}


static char *get_nick(IrcMsg *msg, char *input, char **tok_off) {
  char *tok = strtok_r(input, "!", tok_off);
  if (!tok) return NULL;
  strncpy(msg->nick, tok+1, MAX_NICK_LEN);
  return tok;
}

static char *get_hostname(IrcMsg *msg, char *input, char **tok_off) {
  //skip host name
  char *tok = strtok_r(NULL, " ", tok_off);
  return tok;
}

static char *get_action(IrcMsg *msg, char *input, char **tok_off) {
  char *tok = strtok_r(NULL, " ", tok_off);
  if (!tok) return NULL;
  strncpy(msg->action, tok, MAX_CMD_LEN);
  return tok;
}


IrcMsg *ircMsg_irc_new(char *input) {
  IrcMsg *msg = ircMsg_newMsg();
  char *end = input + strlen(input);
  char *tok = NULL, *tok_off = NULL;

  if (!(tok = get_nick(msg, input, &tok_off))) return msg;
  if (!(tok = get_hostname(msg, input, &tok_off))) return msg;
  if (!(tok = get_action(msg, input, &tok_off))) return msg;

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
  return msg;
}

IrcMsg *ircMsg_server_new(char *input) {
  IrcMsg *msg = ircMsg_newMsg();
  int i = 0;
  char *end = input + strlen(input);
  char *tok = NULL, *tok_off = NULL;

  msg->server = 1;
  //skip the server
  tok = strtok_r(input, SERVER_INFO_DELIM, &tok_off);
  if (!tok) return msg;

  //copy the status code
  tok = strtok_r(NULL, SERVER_INFO_DELIM, &tok_off);
  if (!tok) return msg;
  strncpy(msg->action, tok, MAX_CMD_LEN);

  //skip the name the server issued the command to
  tok = strtok_r(NULL, SERVER_INFO_DELIM, &tok_off);
  if (!tok || !tok_off || tok_off + 1 >= end)
  	return msg;

  //look for channel if any exits
  char *channel = tok_off;
  if (*channel == '@' || *channel == '=') {
  	channel++;
  	tok = strtok_r(channel, SERVER_INFO_DELIM":", &tok_off);
  	ircMsg_setChannel(msg, tok);
  }

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


