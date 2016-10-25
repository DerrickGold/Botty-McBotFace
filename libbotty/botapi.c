#include <stdio.h>
#include "botapi.h"

static int ircRefCount = 0;

//initialize the bot using data set in *bot
int botty_init(BotInfo *bot, int argc, char *argv[], int argstart) {
  //keep track of irc singleton references are used, so that when
  //we are freeing bottys, we can clear up the shared irc data.
  ircRefCount += (irc_init() == 0);
  return bot_init(bot, argc, argv, argstart);
}


void botty_cleanup(BotInfo *bot) {
  bot_cleanup(bot);
  //clean up shared irc data when there are no more
  //bottys alive.
  if (--ircRefCount == 0) irc_cleanup();
}



