#ifndef __BOT_API_H__
#define __BOT_API_H__

#include <poll.h>

#include "globals.h"
#include "callback.h"
#include "irc.h"
#include "cmddata.h"
#include "builtin.h"
#include "botprocqueue.h"
#include "commands.h"

int botty_init(BotInfo *bot, int argc, char *argv[], int argstart);

void botty_cleanup(BotInfo *bot);

void botty_runProcess(BotInfo *bot, BotProcessFn fn, BotProcessArgs *args, char *cmd, char *caller);

void botty_addCommand(BotInfo *bot, char *cmd, int flags, int args, CommandFn fn);

#define botty_join(bot, channel) \
	bot_join(bot, channel)

//returns void
#define botty_setCallback(bot, id, fn)          \
  bot_setCallback(bot, id, fn)

//returns int, negative value indicates error
#define botty_connect(bot) \
  bot_connect(bot)

//returns int, negative value indicates exit on error
#define botty_process(bot) \
  bot_run(bot)

//returns int based on success
#define botty_send(bot, target, action, ctcp, fmt, ...)  \
  bot_send(bot, target, action, ctcp, fmt, ##__VA_ARGS__)

#define botty_say(bot, target, fmt, ...) \
  bot_send(bot, target, ACTION_MSG, NULL, fmt, ##__VA_ARGS__)

//returns int, negative value indicates error
#define botty_ctcpSend(bot, target, command, msg, ...) \
  bot_send(bot, target, ACTION_MSG, command, msg, ##__VA_ARGS__)

//bot_ctcp_send(bot, target, command, msg, ##__VA_ARGS__)

//returns char * pointer to where the bot should write its output
//from a given message (either a particular user in private message, or
//the channel the bot is residing in).
#define botty_respondDest(cmddata) \
  botcmd_builtin_getTarget(cmddata)

#define botty_isThrottled(bot) \
  bot_isThrottled(bot)

#define botty_makeProcessArgs(data, target, fn) \
  BotProcess_makeArgs(data, target, fn)

#endif //__BOT_API_H__
