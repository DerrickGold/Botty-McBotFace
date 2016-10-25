#ifndef __BOT_API_H__
#define __BOT_API_H__

#include <poll.h>

#include "globals.h"
#include "callback.h"
#include "irc.h"
#include "cmddata.h"

extern int botty_init(BotInfo *bot, int argc, char *argv[], int argstart);

extern void botty_cleanup(BotInfo *bot);

//returns void
#define botty_addCommand(bot, cmd, flags, args, fn) \
  bot_addcommand(bot, cmd, flags, args, fn)

//returns void
#define botty_setGlobalCallback(id, fn) \
  callback_set(id, fn)

//returns int, negative value indicates error
#define botty_connect(bot) \
  bot_connect(bot)

//returns int, negative value indicates exit on error
#define botty_process(bot) \
  bot_run(bot)

//returns int based on success
#define botty_send(bot, target, fmt, ...) \
  botSend(bot, target, fmt, ##__VA_ARGS__)

//returns int, negative value indicates error
#define botty_ctcpSend(bot, target, command, msg, ...) \
  ctcpSend(bot, target, command, msg, ##__VA_ARGS__)

//returns char * pointer to where the bot should write its output
//from a given message (either a particular user in private message, or
//the channel the bot is residing in).
#define botty_respondDest(cmddata) \
  botcmd_getTarget(cmddata)


#endif //__BOT_API_H__