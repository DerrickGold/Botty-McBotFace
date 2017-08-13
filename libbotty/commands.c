#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "commands.h"

/*
 * Register a command for the bot to use
 */
int command_reg(HashTable *cmdTable, char *cmdtag, int flags, int args, CommandFn fn) {
  if (!cmdTable || !cmdtag || !fn) {
    fprintf(stderr, "Command registration failed:null table, tag, or function given\n");
    return -1;
  }

  BotCmd *newcmd = calloc(1, sizeof(BotCmd));
  if (!newcmd) {
    perror("Command Alloc Error: ");
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
    fprintf(stderr, "Error adding command %s to hash\n", cmdtag);
    return -3;
  }
  return 0;
}

BotCmd *command_get(HashTable *cmdTable, char *command) {
  HashEntry *e = HashTable_find(cmdTable, command);
  if (e) return (BotCmd *)e->data;
  return NULL;
}

int command_call_r(BotCmd *cmd, void *data, char *args[MAX_BOT_ARGS]) {
if (!cmd) {
    fprintf(stderr, "Command (%s) is not a registered command\n", cmd->cmd);
    return -1;
  }
  return cmd->fn(data, args);
}

int command_call(HashTable *cmdTable, char *command, void *data, char *args[MAX_BOT_ARGS]) {
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

void command_cleanup(HashTable *cmdTable) {
  HashTable_forEach(cmdTable, NULL, &cleanCmd);
  HashTable_destroy(cmdTable);
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
    fprintf(stderr, "Command Alias registration failed:null table, alias, or command given\n");
    return -1;
  }

  //first check that the alias doesn't already exist or map to an existing command
  BotCmd *checkCmd = command_get(cmdTable, alias);
  if (checkCmd) {
    fprintf(stderr, "Cannot override an existing command with an alias: %s\n", alias);
    return ALIAS_ERR_CMDEXISTS;
  }

  CmdAlias *checkAlias = command_alias_get(cmdAliases, alias);
  if (checkAlias) {
    fprintf(stderr, "Alias for %s already exists.\n", alias);
    return ALIAS_ERR_ALREADYEXISTS;
  }

  size_t keyLen = strlen(alias);
  char *key = calloc(1, keyLen + 1);
  if (!key) {
    perror("Alias key allocation error: ");
    return -1;
  }
  strncpy(key, alias, keyLen);


  CmdAlias *aliasData = calloc(1, sizeof(CmdAlias));
  if (!aliasData) {
    perror("Failed to allocate CmdAlias Struct: ");
    return -1;
  }

  //copy the text we plan on replacing with
  size_t cmdLen = strlen(cmd);
  aliasData->replaceWith = calloc(1, cmdLen + 1);
  if (!aliasData->replaceWith) {
    free(key);
    perror("Alias command allocation error: ");
    return -1;
  }
  strncpy(aliasData->replaceWith, cmd, cmdLen);
  char *replaceWithEnd = aliasData->replaceWith + cmdLen;


  char *tok = aliasData->replaceWith;
  char *tok_off = strchr(tok, BOT_ARG_DELIM);
  if (tok_off) *tok_off = '\0';

  BotCmd *regCmd = command_get(cmdTable, tok);
  if (!regCmd) {
    fprintf(stderr, "Cannot alias command that doesn't exist: %s\n", tok);
    return ALIAS_ERR_CMDNOTFOUND;
  }

  aliasData->cmd = regCmd;
  aliasData->args[0] = aliasData->cmd->cmd;
  fprintf(stderr, "ALIAS ARG[0]: %s\n", aliasData->args[0]);
  if (tok_off) {
    tok = tok_off + 1;
    int i = 1;
    aliasData->argc = 1;
    while(tok < replaceWithEnd && i < regCmd->args) {
      tok_off = strchr(tok, BOT_ARG_DELIM);
      if (tok_off && i < regCmd->args - 1) *tok_off = '\0';
      aliasData->args[i] = tok;
      fprintf(stderr, "ALIAS ARG[%d]: %s\n", i, aliasData->args[i]);
      aliasData->argc++;
      if (!tok_off) break;
      tok_off++;
      tok = tok_off;
      i++;
    }
  }

  HashEntry *e = HashEntry_create(key, aliasData);
  if (!e) {
    fprintf(stderr, "Error creating hash entry for alias\n");
    free(key);
    command_alias_freeAlias(aliasData);
    return -4;
  }
  if (!HashTable_add(cmdAliases, e)) {
    command_alias_free(e);
    fprintf(stderr, "Error adding alias %s to hash\n", alias);
    return -5;
  }
  return ALIAS_ERR_NONE;
}

CmdAlias *command_alias_get(HashTable *cmdAliases, char *alias) {
  HashEntry *e = HashTable_find(cmdAliases, alias);
  if (e) return (CmdAlias *)e->data;
  return NULL;
}
