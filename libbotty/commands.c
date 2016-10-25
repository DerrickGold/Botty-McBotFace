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

