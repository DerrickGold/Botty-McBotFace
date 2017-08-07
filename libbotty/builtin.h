#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include "irc.h"
#include "cmddata.h"

int botcmd_builtin(BotInfo *bot);
char *botcmd_builtin_getTarget(CmdData *data);

#endif //__BUILTIN_H__
