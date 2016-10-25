#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "globals.h"

typedef enum {
  CMDFLAG_MASTER = (1<<0),
} CommandFlags;

typedef struct BotCmd {
  char *cmd;
  int flags;
  int args;
  int (*fn)(void *, char *a[MAX_BOT_ARGS]);
} BotCmd;

typedef int (*CommandFn)(void *, char *a[MAX_BOT_ARGS]);

extern int command_reg(HashTable *cmdTable, char *cmdtag, int flags, int args, CommandFn fn);

extern BotCmd *command_get(HashTable *cmdTable, char *command);

extern int command_call_r(BotCmd *cmd, void *data, char *args[MAX_BOT_ARGS]);

extern int command_call(HashTable *cmdTable, char *command, void *data, char *args[MAX_BOT_ARGS]);

extern void command_cleanup(HashTable *cmdTable);

#endif //__COMMANDS_H__
