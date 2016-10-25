#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include "irc.h"
#include "cmddata.h"

extern int botcmd_builtin(BotInfo *bot);

extern char *botcmd_getTarget(CmdData *data);

#endif //__BUILTIN_H__
