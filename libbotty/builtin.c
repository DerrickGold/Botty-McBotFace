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

static int botcmd_builtin_help(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  char output[MAX_MSG_LEN] = "Available commands: ";
  char *end = NULL;
  char *target = botcmd_builtin_getTarget(data);

  HashTable_forEach(data->bot->commands, output, &printCmd);
  end = strrchr(output, ',');
  if (end) *end = '\0';
  botty_say(data->bot, target, output);
  return 0;
}

static int botcmd_builtin_info(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  char *target = botcmd_builtin_getTarget(data);
  botty_say(data->bot, target, INFO_MSG);
  return 0;
}

static int botcmd_builtin_source(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  char *target = botcmd_builtin_getTarget(data);
  botty_say(data->bot, target, SRC_MSG);
  return 0;
}

static int botcmd_builtin_die(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  botty_say(data->bot, NULL, "Seeya!");
  bot_irc_send(&data->bot->conInfo, "QUIT :leaving");
  return -1;
}

//A sample 'process' function that can be given to the bot
static int _script(void *b, void *args) {
  BotInfo *bot = (BotInfo *)b;
  FILE *input = (FILE *)args;
  char buf[MAX_MSG_LEN];
  static char throttleBuf[MAX_MSG_LEN];

  struct timespec sleepTimer = {
    .tv_sec = 0,
    .tv_nsec = ONE_SEC_IN_NS / MSG_PER_SECOND_LIM
  };

  struct timespec throttleTimer = {
    .tv_sec = THROTTLE_WAIT_SEC,
    .tv_nsec = 0
  };

  if (feof(input))
    goto _fin;

  if (botty_isThrottled(bot)) {
    fprintf(stderr, "Sleeping due to throttling\n");
    nanosleep(&throttleTimer, NULL);
    if (botty_say(bot, NULL, ". %s", throttleBuf) < 0)
      goto _fin;
  }
  else {
    int ret = 0;
    if (!connection_client_poll(&bot->conInfo, POLLOUT, &ret))
      return 1;

    char *s = fgets(buf, MAX_MSG_LEN, input);
    if (!s)
      goto _fin;

    char *newline = strchr(s, '\n');
    if (newline) *newline = '\0';
    memset(throttleBuf, 0, sizeof(throttleBuf));
    memcpy(throttleBuf, buf, sizeof(buf));
    //strncpy(throttleBuf, s, MAX_MSG_LEN);

    if (botty_say(bot, NULL, ". %s", s) < 0)
      goto _fin;
  }

  nanosleep(&sleepTimer, NULL);
  //return 1 to keep the process going
  return 1;

  _fin:
  //return negative value to indicate the process
  //is complete
  pclose(input);
  return -1;
}

int botcmd_builtin_script(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  char fullCmd[MAX_MSG_LEN + strlen(SCRIPTS_DIR) + strlen(SCRIPT_OUTPUT_REDIRECT)];
  char *cmd = args[1];
  if (!cmd) {
    botty_say(data->bot, NULL, "%s: please specify a command.", data->msg->nick);
    return 0;
  }

  if (cmd[0] == '.' || cmd[0] == '~' || cmd[0] == '\'' || cmd[0] == '\"') {
    botty_say(data->bot, NULL, "%s: invalid command specified.", data->msg->nick);
    return 0;
  }

  snprintf(fullCmd, sizeof(fullCmd), SCRIPTS_DIR"%s"SCRIPT_OUTPUT_REDIRECT, cmd);
  FILE *f = popen(fullCmd, "r");
  if (!f) {
    botty_say(data->bot, NULL, "Script '%s' does not exist!", cmd);
    return 0;
  }
  //initialize and start the draw process
  //A process will block all input for a given bot until
  //it has completed the assigned process.
  bot_setProcess(data->bot, &_script, (void*)f);
  return 0;
}

/*
 * If necessary, returns where the bot received its input
 * as to respond in the appropriate place.
 *
 * For example, if a user private messages a bot,
 * it can respond in the private message as opposed
 * to the channel it is in.
 */
char *botcmd_builtin_getTarget(CmdData *data) {
  char *target = data->msg->channel;
  if (!strcmp(target, data->bot->nick[data->bot->nickAttempt]))
    target = data->msg->nick;

  return target;
}

/*
 * Initialize the built in commands provided in this file.
 */
int botcmd_builtin(BotInfo *bot) {
  bot_addcommand(bot, "help", 0, 1, &botcmd_builtin_help);
  bot_addcommand(bot, "info", 0, 1, &botcmd_builtin_info);
  bot_addcommand(bot, "source", 0, 1, &botcmd_builtin_source);
  bot_addcommand(bot, "die", CMDFLAG_MASTER, 1, &botcmd_builtin_die);
  bot_addcommand(bot, "script", 0, 2, &botcmd_builtin_script);
  return 0;
}
