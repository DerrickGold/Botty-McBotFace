#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "commands.h"
#include "ircmsg.h"

#define CMD_NAME_POS 0

int commands_init(HashTable **commands) {
	if (!commands) {
		syslog(LOG_CRIT, "%s: Null hashtable pointer provided.", __FUNCTION__);
		return -1;
	}

  *commands = HashTable_init(COMMAND_HASH_SIZE);
  if (!*commands) {
    syslog(LOG_CRIT, "%s: Error allocating command hash for bot", __FUNCTION__);
    return -1;
  }

  return 0;
}

/*
 * Register a command for the bot to use
 */
int command_reg(HashTable *cmdTable, char *cmdtag, int flags, int args, CommandFn fn) {
  if (!cmdTable || !cmdtag || !fn) {
    syslog(LOG_CRIT, "Command registration failed:null table, tag, or function given");
    return -1;
  }

  BotCmd *newcmd = calloc(1, sizeof(BotCmd));
  if (!newcmd) {
    syslog(LOG_CRIT, "Failed to allocate new BotCmd object");
    return -1;
  }

  newcmd->args = args;
  newcmd->fn = fn;
  newcmd->flags = flags;
  HashEntry *e = HashEntry_create(cmdtag, newcmd);
  if (!e) {
    free(newcmd);
    return -2;
  }
  newcmd->cmd = e->key;
  if (!HashTable_add(cmdTable, e)) {
    syslog(LOG_CRIT, "Error adding command %s to hash (key:%s)", cmdtag, newcmd->cmd);
    return -3;
  }
  return 0;
}

BotCmd *command_get(HashTable *cmdTable, char *command) {
  HashEntry *e = HashTable_find(cmdTable, command);
  if (e) return (BotCmd *)e->data;
  return NULL;
}

int command_call_r(BotCmd *cmd, CmdData *data, char *args[MAX_BOT_ARGS]) {
	if (!cmd) {
    syslog(LOG_WARNING, "Command (%s) is not a registered command", cmd->cmd);
    return -1;
  }
  return cmd->fn(data, args);
}

int command_call(HashTable *cmdTable, char *command, CmdData *data, char *args[MAX_BOT_ARGS]) {
  BotCmd *cmd = command_get(cmdTable, command);
  return command_call_r(cmd, data, args);
}

static int cleanCmd(HashEntry *entry, void *data) {
  if (entry->data) {
    free(entry->data);
    entry->data = NULL;
  }
  return 0;
}

void command_cleanup(HashTable **cmdTable) {
  HashTable_forEach(*cmdTable, NULL, &cleanCmd);
  HashTable_destroy(*cmdTable);
  *cmdTable = NULL;
}



int command_alias_init(HashTable **cmdaliases) {
	if (!cmdaliases) {
		syslog(LOG_CRIT, "%s: Null hashtable pointer provided.", __FUNCTION__);
		return -1;
	}

  *cmdaliases = HashTable_init(COMMAND_HASH_SIZE);
  if (!*cmdaliases) {
    syslog(LOG_CRIT, "%s: Error allocating cmdaliases hash for bot", __FUNCTION__);
    return -1;
  }

  return 0;
}

void command_alias_freeAlias(CmdAlias *alias) {
  free(alias->replaceWith);
  free(alias);
}

void command_alias_free(HashEntry *entry) {
  free(entry->key);
  command_alias_freeAlias((CmdAlias *)entry->data);
}


int command_reg_alias(HashTable *cmdTable, HashTable *cmdAliases, char *alias, char *cmd) {
  if (!cmdAliases || !alias || !cmd) {
    syslog(LOG_CRIT, "Command Alias registration failed:null table, alias, or command given");
    return -1;
  }

  //first check that the alias doesn't already exist or map to an existing command
  BotCmd *checkCmd = command_get(cmdTable, alias);
  if (checkCmd) {
    syslog(LOG_WARNING, "Cannot override an existing command with an alias: %s", alias);
    return ALIAS_ERR_CMDEXISTS;
  }

  CmdAlias *checkAlias = command_alias_get(cmdAliases, alias);
  if (checkAlias) {
    syslog(LOG_WARNING, "Alias for %s already exists.", alias);
    return ALIAS_ERR_ALREADYEXISTS;
  }

  char *key = strdup(alias);
  if (!key) {
    syslog(LOG_CRIT, "Error allocating alias key: %s", alias);
    return -1;
  }

  CmdAlias *aliasData = calloc(1, sizeof(CmdAlias));
  if (!aliasData) {
    syslog(LOG_CRIT, "Error allocating CmdAlias object");
    return -1;
  }

  //copy the text we plan on replacing with
  aliasData->replaceWith = strdup(cmd);
  if (!aliasData->replaceWith) {
    free(key);
    syslog(LOG_CRIT, "Error allocating space to hold alias command");
    return -1;
  }
  char *replaceWithEnd = aliasData->replaceWith + strlen(cmd);


  char *tok = aliasData->replaceWith;
  char *tok_off = strchr(tok, BOT_ARG_DELIM);
  if (tok_off) *tok_off = '\0';

  BotCmd *regCmd = command_get(cmdTable, tok);
  if (!regCmd) {
    syslog(LOG_WARNING, "Cannot alias command that doesn't exist: %s", tok);
    return ALIAS_ERR_CMDNOTFOUND;
  }

  aliasData->cmd = regCmd;
  aliasData->args[0] = aliasData->cmd->cmd;
  aliasData->argc = 1;
  syslog(LOG_DEBUG, "ALIAS ARG[0]: %s", aliasData->args[0]);
  if (tok_off) {
    tok = tok_off + 1;
    int i = 1;
    while(tok < replaceWithEnd && i < regCmd->args) {
      tok_off = strchr(tok, BOT_ARG_DELIM);
      if (tok_off && i < regCmd->args - 1) *tok_off = '\0';
      aliasData->args[i] = tok;
      syslog(LOG_DEBUG, "ALIAS ARG[%d]: %s", i, aliasData->args[i]);
      aliasData->argc++;
      if (!tok_off) break;
      tok_off++;
      tok = tok_off;
      i++;
    }
  }

  HashEntry *e = HashEntry_create(key, aliasData);
  if (!e) {
    syslog(LOG_CRIT, "Error creating hash entry for alias");
    free(key);
    command_alias_freeAlias(aliasData);
    return -4;
  }
  if (!HashTable_add(cmdAliases, e)) {
    command_alias_free(e);
    syslog(LOG_CRIT, "Error adding alias %s to hash", alias);
    return -5;
  }
  return ALIAS_ERR_NONE;
}

CmdAlias *command_alias_get(HashTable *cmdAliases, char *alias) {
  HashEntry *e = HashTable_find(cmdAliases, alias);
  if (e) return (CmdAlias *)e->data;
  return NULL;
}


#define tokenize() do {                             \
  tok_off = strchr(tok, BOT_ARG_DELIM);             \
  if (tok_off && i < argCount - 1) *tok_off = '\0'; \
  msg->msgTok[i] = tok;                             \
} while(0)

#define nextToken() do {  \
  if (!tok_off)           \
    return cmd;           \
  tok = (++tok_off);      \
} while (0)


BotCmd *command_parse_ircmsg(IrcMsg *msg, HashTable *cmdTable, HashTable *cmdAliases) {
  if (msg->msg[0] != CMD_CHAR)
    return NULL;

  syslog(LOG_DEBUG, "Starting to parse command: %s", msg->msg);
  BotCmd *cmd = NULL;
  CmdAlias *alias = NULL;
  int argCount = MAX_BOT_ARGS;
  char *tok = msg->msg + 1;
  char *tok_off = NULL;
  int i = 0;

  tokenize();
  //check first if word is a registered command
  if ((cmd = command_get(cmdTable, msg->msgTok[CMD_NAME_POS]))) {
    argCount = cmd->args;
    i++;
  }
  //then check if its an alias if it is not
  else if ((alias= command_alias_get(cmdAliases, msg->msgTok[CMD_NAME_POS]))) {
    cmd = alias->cmd;
    argCount = alias->cmd->args;

    for (i = 0; i < alias->argc; i++)
      msg->msgTok[i] = alias->args[i];
  }
  nextToken();

  while(i < argCount) {
    tokenize();
    nextToken();
    i++;
  }
  syslog(LOG_DEBUG, "Done tokenizing command");
  return cmd;
}


