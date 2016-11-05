/*
 * Bot built in commands. These are commands that must
 * be available for use in any bot spawned.
 */
#include <stdio.h>
#include <string.h>
#include <poll.h>

#include "builtin.h"
#include "globals.h"
#include "botapi.h"

/*
 * Default commands that should be available for 
 * for all bots.
 */
static int printCmd(HashEntry *entry, void *output) {
  strncat((char *)output, entry->key, MAX_MSG_LEN);
  strncat((char *)output, ", ", MAX_MSG_LEN);
  return 0;
}

int botcmd_help(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  char output[MAX_MSG_LEN] = "Available commands: ";
  char *end = NULL;
  char *target = botcmd_getTarget(data);
  
  HashTable_forEach(data->bot->commands, output, &printCmd);
  end = strrchr(output, ',');
  if (end) *end = '\0';
  botty_say(data->bot, target, output);
  return 0;
}

int botcmd_info(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  char *target = botcmd_getTarget(data);
  botty_say(data->bot, target, INFO_MSG);
  return 0;
}

int botcmd_source(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  char *target = botcmd_getTarget(data);
  botty_say(data->bot, target, SRC_MSG);
  return 0;
}

int botcmd_die(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  botty_say(data->bot, NULL, "Seeya!");
  ircSend(data->bot->servfds.fd, "QUIT :leaving");
  return -1;
}

/*
 * If necessary, returns where the bot received its input
 * as to respond in the appropriate place.
 *
 * For example, if a user private messages a bot,
 * it can respond in the private message as opposed
 * to the channel it is in.
 */
char *botcmd_getTarget(CmdData *data) {
  char *target = data->msg->channel;
  if (!strcmp(target, data->bot->nick[data->bot->nickAttempt]))
    target = data->msg->nick;
  
  return target;
}

/*
 * Initialize the built in commands provided in this file.
 */
int botcmd_builtin(BotInfo *bot) {
  bot_addcommand(bot, "help", 0, 1, &botcmd_help);
  bot_addcommand(bot, "info", 0, 1, &botcmd_info);
  bot_addcommand(bot, "source", 0, 1, &botcmd_source);  
  bot_addcommand(bot, "die", CMDFLAG_MASTER, 1, &botcmd_die);
  return 0;
}
