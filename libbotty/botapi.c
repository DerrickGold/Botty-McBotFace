#include <stdio.h>
#include <libgen.h>
#include "botapi.h"
#include "commands.h"

static int ircRefCount = 0;
static char runDirectory[MAX_FILEPATH_LEN];


//initialize the bot using data set in *bot
int botty_init(BotInfo *bot, int argc, char *argv[], int argstart) {

  realpath(argv[0], runDirectory);
  char *pathDir = dirname(runDirectory);
  snprintf(runDirectory, MAX_FILEPATH_LEN - 1, "%s", pathDir);

  //keep track of irc singleton references are used, so that when
  //we are freeing bottys, we can clear up the shared irc data.
  ircRefCount += (bot_irc_init() == 0);
  return bot_init(bot, argc, argv, argstart);
}

char *botty_getDirectory(void) {
  return runDirectory;
}


void botty_addCommand(BotInfo *bot, char *cmd, int flags, int args, CommandFn fn) {
  command_reg(bot->commands, cmd, flags, args, fn);
}

void botty_cleanup(BotInfo *bot) {
  bot_cleanup(bot);
  //clean up shared irc data when there are no more
  //bottys alive.
  if (--ircRefCount == 0) bot_irc_cleanup();
}


void botty_runProcess(BotInfo *bot, BotProcessFn fn, BotProcessArgs *args, char *cmd, char *caller) {
  bot_runProcess(bot, fn, args, cmd, caller);
}
