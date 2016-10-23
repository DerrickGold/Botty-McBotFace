#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "hash.h"

#define INFO_MSG \
  "Created by Derrick Gold. Compiled at "__TIME__", "__DATE__

#define SRC_MSG \
  "Source Code: git clone https://github.com/DerrickGold/Botty-McBotFace.git"

#define MSG_LEN_EXCEED \
  "Message length exceeds %d byte limit. Message splitting not supported yet\n"

#define SERVER_PREFIX "irc."
#define CMD_CHAR '~'
#define PARAM_DELIM ':'
#define BOT_ARG_DELIM ' '
#define MSG_FOOTER "\r\n"
#define CTCP_MARKER "\x01"
#define POLL_TIMEOUT_MS 100

//number of alternative nicks and attempts the bot should try
//before giving up registering to the server
#define NICK_ATTEMPTS 3

#define MAX_MSG_LEN 512
#define MAX_SERV_LEN 63
#define MAX_NICK_LEN 30
#define MAX_CHAN_LEN 50
#define MAX_CMD_LEN 9
#define MAX_BOT_ARGS 8
#define MAX_PARAMETERS 15

#define REG_SUC_CODE "001"
#define NAME_REPLY "353"
#define REG_ERR_CODE "433"

#define ACTION_HASH_SIZE 43

typedef enum {
  IRC_ACTION_NOP = 0, IRC_ACTION_DIE, IRC_ACTION_WHO, IRC_ACTION_KICK, IRC_ACTION_NICK,
  IRC_ACTION_MODE, IRC_ACTION_INFO, IRC_ACTION_KILL, IRC_ACTION_PING,
  IRC_ACTION_TIME, IRC_ACTION_JOIN, IRC_ACTION_AWAY, IRC_ACTION_MOTD,
  IRC_ACTION_PONG, IRC_ACTION_OPER, IRC_ACTION_PART, IRC_ACTION_ISON,
  IRC_ACTION_LIST, IRC_ACTION_USER, IRC_ACTION_QUIT, IRC_ACTION_ADMIN,
  IRC_ACTION_TRACE, IRC_ACTION_NAMES, IRC_ACTION_TOPIC, IRC_ACTION_LINKS,
  IRC_ACTION_ERROR, IRC_ACTION_WHOIS, IRC_ACTION_STATS, IRC_ACTION_USERS,
  IRC_ACTION_SQUIT, IRC_ACTION_REHASH, IRC_ACTION_INVITE, IRC_ACTION_WHOWAS,
  IRC_ACTION_LUSERS, IRC_ACTION_SUMMON, IRC_ACTION_SQUERY, IRC_ACTION_CONNECT,
  IRC_ACTION_SERVICE, IRC_ACTION_WALLOPS, IRC_ACTION_RESTART, IRC_ACTION_VERSION,
  IRC_ACTION_SERVLIST, IRC_ACTION_USERHOST,
  API_ACTION_COUNT
} IRC_API_Actions;

extern const char IrcApiActionText[API_ACTION_COUNT][MAX_CMD_LEN];
extern HashTable *IrcApiActions;

#endif //__GLOBALS_H__
