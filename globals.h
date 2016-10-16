#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#define MSG_LEN_EXCEED \
  "Message length exceeds %d byte limit. Message splitting not supported yet\n"

#define SERVER_PREFIX "irc."
#define CMD_CHAR '~'
#define MAX_MSG_LEN 512
#define MAX_SERV_LEN 63
#define MAX_NICK_LEN 30
#define MAX_CHAN_LEN 50
#define MAX_CMD_LEN 9
#define BOT_ARG_DELIM ' '
#define MAX_BOT_ARGS 8

#define REG_ERR_CODE "433"
#define REG_SUC_CODE "001"


#endif //__GLOBALS_H__
