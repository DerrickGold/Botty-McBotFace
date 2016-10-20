#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "commands.h"

static BotCmd *cmds = NULL;

BotCmd *command_global(void) {
  return cmds;
}

/*
 * Register a command for the bot to use
 */
void command_reg_r(BotCmd **commands, char *cmdtag, int flags, int args, CommandFn fn) {
  if (!commands) return;

  BotCmd *curCmd;
  BotCmd *newcmd = calloc(1, sizeof(BotCmd));
  if (!newcmd) {
    perror("Command Alloc Error: ");
    exit(1);
  }
  
  strncpy(newcmd->cmd, cmdtag, MAX_CMD_LEN);
  newcmd->args = args;
  newcmd->fn = fn;
  newcmd->flags = flags;
  
  if (!*commands) {
    //first command
    *commands = newcmd;
    return;
  }
  
  curCmd = *commands;
  while (curCmd->next) curCmd = curCmd->next;
  curCmd->next = newcmd;
}


void command_reg(char *cmdtag, int flags, int args, CommandFn fn) {
  return command_reg_r(&cmds, cmdtag, flags, args, fn);
}


BotCmd *command_get_r(BotCmd *commands, char *command) {
  BotCmd *curcmd = commands;
  while (curcmd && strncmp(curcmd->cmd, command, MAX_CMD_LEN))
    curcmd = curcmd->next;

  return curcmd;
}


BotCmd *command_get(char *command) {
  return command_get_r(cmds, command);
}


int command_call_r(BotCmd *commands, char *command, void *data, char *args[MAX_BOT_ARGS]) {

  BotCmd *curcmd = command_get_r(commands, command);
  if (!curcmd) {
    fprintf(stderr, "Command (%s) is not a registered command\n", command);
    return -1;
  }
  
  return curcmd->fn(data, args);
}

int command_call(char *command, void *data, char *args[MAX_BOT_ARGS]) {
  return command_call_r(cmds, command, data, args);
}


void command_cleanup_r(BotCmd **commands) {
  BotCmd *cur = *commands;
  BotCmd *next = NULL;
  while (cur) {
    next = cur->next;
    free(cur);
    cur = next;
  }
}

void command_cleanup(void) {
  if (cmds) command_cleanup_r(&cmds);
}

