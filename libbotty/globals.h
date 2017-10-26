#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <syslog.h>
#include <time.h>
#include "hash.h"

#define ALIAS_FILE_PATH "aliases.txt"

#define INFO_MSG \
  "Created by Derrick Gold. Compiled at "__TIME__", "__DATE__

#define SRC_MSG \
  "Source Code: git clone https://github.com/DerrickGold/Botty-McBotFace.git"

#define MSG_LEN_EXCEED \
  "Message length exceeds %d byte limit. Message splitting not supported yet\n"

#define DEFAULT_CONFIG_FILE "settings.json"

#define ILLEGAL_NICK_CHARS "-&@+%"
#define CHANNEL_START_CHAR '#'
#define SERVER_PREFIX "irc."
#define CMD_CHAR '~'
#define PARAM_DELIM ':'
#define PARAM_DELIM_STR ":"
#define BOT_ARG_DELIM ' '
#define SERVER_INFO_DELIM " "
#define ARG_DELIM_LEN 1
#define NEWLINE_CHR '\n'
#define STREND_CHR '\0'
#define MSG_FOOTER "\r\n"
#define CTCP_MARKER "\x01"
#define ACTION_MSG "PRIVMSG"
#define ACTION_EMPTY ""
#define POLL_TIMEOUT_MS 100
#define SCRIPTS_DIR "scripts/"
#define SCRIPT_OUTPUT_REDIRECT " 2>&1 &"
#define SCRIPT_OUTPUT_MODE_TOKEN "#__NOTIFY_ALL__#"
#define SCRIPT_OUTPUT_REDIRECT_TOKEN "#__PRIVATE_MSG__#"
#define SCRIPT_OUTPUT_BOTINPUT_TOKEN "#__BOTINPUT__#"
#define SCRIPT_OUTPIT_DELIM "\n\r\0"
#define INPUT_SPOOFED_HOSTNAME "SpoofedInput"

#define ONE_SEC_IN_NS 999999999
#define ONE_SEC_IN_US 1000000
#define ONE_SEC_IN_MS 1000LL
#define MSG_PER_SECOND_LIM 4
#define THROTTLE_WAIT_SEC 5
#define MAX_RUNNING_SCRIPTS 50

#define THROTTLE_NEEDLE "throttl"

//number of alternative nicks and attempts the bot should try
//before giving up registering to the server
#define NICK_ATTEMPTS 3

#define MAX_FILEPATH_LEN 4096
#define MAX_CONNECTED_CHANS 5
#define MAX_MSG_LEN 512
#define MAX_MSG_SPLITS 4
#define MAX_SERV_LEN 63
#define MAX_NICK_LEN 30
#define MAX_CHAN_LEN 50
#define MAX_CMD_LEN 9
#define MAX_BOT_ARGS 8
#define MAX_PARAMETERS 15
#define MAX_PORT_LEN 6
#define MAX_HOST_LEN 256
#define MAX_IDENT_LEN 10
#define MAX_REALNAME_LEN 64

#define REG_SUC_CODE "001"
#define POST_REG_MSG1 "002"
#define POST_REG_MSG2 "003"
#define POST_REG_MSG3 "004"
#define NAME_REPLY "353"
#define REG_ERR_CODE "433"
#define NOTICE_ACTION "NOTICE"
#define PING_STR "PING"
#define PONG_STR "PONG"
#define NICK_CMD_STR "NICK"
#define USER_CMD_STR "USER"
#define JOIN_CMD_STR "JOIN"

#define ACTION_HASH_SIZE 43
#define COMMAND_HASH_SIZE 13
#define QUEUE_HASH_SIZE 13
#define ALIAS_HASH_SIZE 13
#define CHANNICKS_HASH_SIZE 13
#define WHITELIST_HASH_SIZE 13

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

const char IrcApiActionText[API_ACTION_COUNT][MAX_CMD_LEN];
HashTable *IrcApiActions;

typedef long long TimeStamp_t;


#define botty_currentTimestamp() ({ \
  struct timeval te; \
  gettimeofday(&te, NULL); \
  TimeStamp_t milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; \
  milliseconds; \
})

#endif //__GLOBALS_H__
