#ifndef __CMDDATA_H__
#define __CMDDATA_H__

#include "ircmsg.h"
#include "irc.h"

typedef struct CmdData {
  BotInfo *info;
  IrcMsg *msg;
} CmdData;


#endif //__CMDDATA_H__
