/*
 * Bot built in commands. These are commands that must
 * be available for use in any bot spawned.
 */
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "builtin.h"
#include "globals.h"
#include "botapi.h"
#include "botprocqueue.h"
#include "botmsgqueues.h"


typedef struct ScriptPtr {
  FILE *fh;
  int fd;
  char notify;
} ScriptPtr;

typedef struct ClearQueueContainer{
	HashTable *msgQueues;
	unsigned int pid;
} ClearQueueContainer;


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
  //botty_say(data->bot, NULL, "Seeya!");
  bot_irc_send(data->bot, "QUIT :leaving");
  return -1;
}


static int _freeScriptArgs(void *args) {
  ScriptPtr *scriptFile = (ScriptPtr *)args;
  pclose(scriptFile->fh);
  free(args);
  return 0;
}

//A sample 'process' function that can be given to the bot
static int _script(void *b, BotProcessArgs *sArgs) {
  BotInfo *bot = (BotInfo *)b;
  ScriptPtr *fptr= (ScriptPtr *)sArgs->data;
  char *responseTarget = sArgs->target;
  char buf[MAX_MSG_LEN + 1];
  memset(buf, 0, sizeof(buf));

  ssize_t r = read(fptr->fd, buf, MAX_MSG_LEN);
  if (r == -1 && errno == EAGAIN) {
    //no data
    return 1;
  }
  else if (r > 0) {
    char *start = strtok(buf, SCRIPT_OUTPIT_DELIM);
    while (start) {
      if (!strncmp(start, SCRIPT_OUTPUT_MODE_TOKEN, MAX_MSG_LEN)) {
        fptr->notify = (fptr->notify + 1) % 2;
      } else {
        if (fptr->notify) {
          if (botty_send(bot, responseTarget, NOTICE_ACTION, NULL, "%s", start) < 0)
            goto _fin;
        }
        else if (botty_say(bot, responseTarget, "%s", start) < 0)
          goto _fin;
      }
      start = strtok(NULL, SCRIPT_OUTPIT_DELIM);
    }
    return 1;
  }

  _fin:
  //return negative value to indicate the process
  //is complete
  BotProcess_freeArgs(sArgs);
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
  if (scriptArgs)
    snprintf(fullCmd, cmdLen, SCRIPTS_DIR"%s %s %s"SCRIPT_OUTPUT_REDIRECT, script, caller, scriptArgs);
  else
    snprintf(fullCmd, cmdLen, SCRIPTS_DIR"%s %s"SCRIPT_OUTPUT_REDIRECT, script, caller);

  FILE *f = popen(fullCmd, "r");
  if (!f) {
    botty_say(data->bot, responseTarget, "Script '%s' does not exist!", script);
    return 0;
  }

  int fd = fileno(f);
  fcntl(fd, F_SETFL, O_NONBLOCK);


  ScriptPtr *scriptFile = calloc(1, sizeof(ScriptPtr));
  if (!scriptFile) {
    botty_say(data->bot, responseTarget, "Error allocating memory for scriptFile ptr.");
    pclose(f);
    return 0;
  }
  scriptFile->fh = f;
  scriptFile->fd = fd;

  BotProcessArgs *sArgs = BotProcess_makeArgs((void *)scriptFile, botcmd_builtin_getTarget(data), &_freeScriptArgs);
  if (!sArgs) {
    botty_say(data->bot, responseTarget, "There was an error allocating memory to execute command: %s", script);
    pclose(f);
    return 0;
  }

  bot_runProcess(data->bot, &_script, sArgs, script, caller);
  //botty_say(data->bot, responseTarget, "%s: started '%s' with pid: %d.", caller, script, pid);
  return 0;
}

static int _listProcesses(void *b, BotProcessArgs *pArgs) {
  BotInfo *bot = (BotInfo *)b;
  BotProcess *proc = (BotProcess *)pArgs->data;
  char *responseTarget = pArgs->target;

  if (!proc)
    goto _fin;


  char *s = proc->details;
  if (botty_say(bot, responseTarget, "%s", s) < 0)
    goto _fin;

  pArgs->data = (void *)proc->next;
  //return 1 to keep the process going
  return 1;

  _fin:
  //return negative value to indicate the process
  //is complete
  BotProcess_freeArgs(pArgs);
  return -1;
}

int botcmd_builtin_listProcesses(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;

  char *script = "[BuiltIn] List Process";
  char *caller = data->msg->nick;
  char *responseTarget = botcmd_builtin_getTarget(data);

  if (!data->bot->procQueue.head) {
    botty_say(data->bot, responseTarget, "%s: There are no running processes", caller);
    return 0;
  }

  BotProcessArgs *pArgs = BotProcess_makeArgs((void*)data->bot->procQueue.head, responseTarget, NULL);
  if (!pArgs) {
    botty_say(data->bot, responseTarget, "There was an error allocating memory to execute command: %s", script);
    return 0;
  }

  bot_runProcess(data->bot, &_listProcesses, pArgs, script, caller);
  return 0;
}


static int _clearQueueHelper(HashEntry *entry, void *clearQueueContainer) {
	ClearQueueContainer *container = clearQueueContainer;
	return BotMsgQueue_rmPidMsg(container->msgQueues, entry->key, container->pid);
}

int botcmd_builtin_killProcess(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;

  char *caller = data->msg->nick;
  char *responseTarget = botcmd_builtin_getTarget(data);

  if (!args[1]) {
    botty_say(data->bot, responseTarget, "%s: Please specify a PID to terminate.", caller);
    return 0;
  }

  unsigned int pid = atoi(args[1]);
  if (!pid) {
    botty_say(data->bot, responseTarget, "%s: Invalid PID specified.", caller);
    return 0;
  }

  ClearQueueContainer container = {data->bot->msgQueues, pid};
  int cleared = HashTable_forEach(data->bot->msgQueues, (void *)&container, &_clearQueueHelper);
  fprintf(stderr, "Cleared %d pid messages from queue\n", cleared);

  BotProcess *toTerminate = BotProcess_findProcessByPid(&data->bot->procQueue, pid);
  if (!toTerminate && !cleared) {
    botty_say(data->bot, responseTarget, "%s: Failed to find process with PID: %d.", caller, pid);
    return 0;
  }
  else if (toTerminate)
  	BotProcess_terminate(toTerminate);

  botty_say(data->bot, responseTarget, "%s: terminated process with PID: %d.", caller, pid);
  return 0;
}


int botcmd_builtin_registerAlias(void *cmdData, char *args[MAX_BOT_ARGS]) {
	CmdData *data = (CmdData *)cmdData;

	char *caller = data->msg->nick;
  	char *responseTarget = botcmd_builtin_getTarget(data);

  	if (!args[1]) {
  		botty_say(data->bot, responseTarget, "%s: Alias format follows: [alias] [text to alias] -> Missing arguments", caller);
  		return 0;
  	}

  	if (!args[2]) {
  		botty_say(data->bot, responseTarget, "%s: Missing text to alias to: %s", caller, args[1]);
  		return 0;
  	}

  	if (!bot_registerAlias(data->bot, args[1], args[2])) {
  		//botty_say(data->bot, responseTarget, "%s: Aliased '%s' -> '%s'", caller, args[1], args[2]);
  		CmdAlias *alias = command_alias_get(data->bot->cmdAliases, args[1]);
  		if (alias) {
  			char argList[MAX_MSG_LEN];
  			memset(argList, 0, MAX_MSG_LEN - 1);
  			size_t offset = 0;
  			for (int i = 0; i < alias->argc; i++) {
  				offset += snprintf(argList + offset, MAX_MSG_LEN - offset, "%s, ", alias->args[i]);
  			}
  			botty_say(data->bot, responseTarget, "%s: Aliased '%s' -> '%s'", caller, args[1], argList);
  		}
  		return 0;
  	}
  	botty_say(data->bot, responseTarget, "%s: Failed to create alias for '%s' -> '%s'", caller, args[1], args[2]);
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
  bot_addcommand(bot, "ps", 0, 1, &botcmd_builtin_listProcesses);
  bot_addcommand(bot, "kill", 1, 2, &botcmd_builtin_killProcess);
  bot_addcommand(bot, "alias", 0, 3, &botcmd_builtin_registerAlias);
  return 0;
}
