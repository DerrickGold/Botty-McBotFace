/*
 * Bot built in commands. These are commands that must
 * be available for use in any bot spawned.
 */
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/stat.h>

#include "builtin.h"
#include "globals.h"
#include "botapi.h"

typedef struct ScriptArgs {
	FILE *pFile;
	char *target;
} ScriptArgs;

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

static ScriptArgs *_setScriptArgs(FILE *pFile, char *responseTarget) {
  ScriptArgs *args = calloc(1, sizeof(ScriptArgs));
  if (!args) return NULL;

  args->pFile = pFile;
  if (responseTarget) {
		size_t responseTargetLen = strlen(responseTarget);
  	args->target = calloc(1, responseTargetLen + 1);
  	if (!args->target) return NULL;
  	strncpy(args->target, responseTarget, responseTargetLen);
  }

  return args;
}

static void _freeScriptArgs(ScriptArgs *args) {
	if (args->target) {
		free(args->target);
		args->target = NULL;
	}
	pclose(args->pFile);
	free(args);
}

//A sample 'process' function that can be given to the bot
static int _script(void *b, void *args) {
  BotInfo *bot = (BotInfo *)b;
  ScriptArgs *sArgs = (ScriptArgs *)args;
  FILE *input = sArgs->pFile;
  char *responseTarget = sArgs->target;

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
    if (botty_say(bot, responseTarget, ". %s", throttleBuf) < 0)
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

    if (botty_say(bot, responseTarget, ". %s", s) < 0)
      goto _fin;
  }

  nanosleep(&sleepTimer, NULL);
  //return 1 to keep the process going
  return 1;

  _fin:
  //return negative value to indicate the process
  //is complete
  _freeScriptArgs(sArgs);
  return -1;
}

int botcmd_builtin_script(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;

  char *script = args[1];
  char *scriptArgs = args[2];
  char *caller = data->msg->nick;
  char *responseTarget = botcmd_builtin_getTarget(data);

  size_t cmdLen = MAX_MSG_LEN + strlen(SCRIPTS_DIR) + strlen(caller) + strlen(SCRIPT_OUTPUT_REDIRECT);
  char fullCmd[cmdLen];

  if (!script || script[0] == '.' || script[0] == '~' || script[0] == '\'' || script[0] == '\"') {
    botty_say(data->bot, responseTarget, "%s: invalid command specified.", data->msg->nick);
    return 0;
  }

  //test existance of script
  struct stat st = {};
  snprintf(fullCmd, cmdLen, SCRIPTS_DIR"%s", script);
  if (stat(fullCmd, &st) == -1) {
  	botty_say(data->bot, responseTarget, "%s: script does not exist.", data->msg->nick);
  	return 0;
  }

  snprintf(fullCmd, cmdLen, SCRIPTS_DIR"%s %s %s"SCRIPT_OUTPUT_REDIRECT, script, caller, scriptArgs);
  FILE *f = popen(fullCmd, "r");
  if (!f) {
    botty_say(data->bot, responseTarget, "Script '%s' does not exist!", script);
    return 0;
  }

  ScriptArgs *sArgs = _setScriptArgs(f, botcmd_builtin_getTarget(data));
  if (!sArgs) {
  	botty_say(data->bot, responseTarget, "There was an error allocating memory to execute command: %s", script);
  	pclose(f);
  	return 0;
  }
  //initialize and start the draw process
  //A process will block all input for a given bot until
  //it has completed the assigned process.
  bot_setProcess(data->bot, &_script, (void*)sArgs);
  return 0;
}


/*
 * Initialize the built in commands provided in this file.
 */
int botcmd_builtin(BotInfo *bot) {
  bot_addcommand(bot, "help", 0, 1, &botcmd_builtin_help);
  bot_addcommand(bot, "info", 0, 1, &botcmd_builtin_info);
  bot_addcommand(bot, "source", 0, 1, &botcmd_builtin_source);
  bot_addcommand(bot, "die", CMDFLAG_MASTER, 1, &botcmd_builtin_die);
  bot_addcommand(bot, "script", 0, 3, &botcmd_builtin_script);
  return 0;
}
