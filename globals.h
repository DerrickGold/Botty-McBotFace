#ifndef __GLOBALS_H__
#define __GLOBALS_H__

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

#endif //__GLOBALS_H__
