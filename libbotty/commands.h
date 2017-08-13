#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "globals.h"

#define ALIAS_ERR_CMDEXISTS -7
#define ALIAS_ERR_CMDNOTFOUND -3
#define ALIAS_ERR_ALREADYEXISTS -8
#define ALIAS_ERR_NONE 0

typedef enum {
  CMDFLAG_MASTER = (1<<0),
} CommandFlags;

typedef struct BotCmd {
  char *cmd;
  int flags;
  int args;
  int (*fn)(void *, char *a[MAX_BOT_ARGS]);
} BotCmd;

typedef struct CmdAlias {
	BotCmd *cmd;
	char *args[MAX_BOT_ARGS];
	int argc;
	char *replaceWith;
} CmdAlias;

typedef int (*CommandFn)(void *, char *a[MAX_BOT_ARGS]);

int command_reg(HashTable *cmdTable, char *cmdtag, int flags, int args, CommandFn fn);

BotCmd *command_get(HashTable *cmdTable, char *command);

int command_call_r(BotCmd *cmd, void *data, char *args[MAX_BOT_ARGS]);

int command_call(HashTable *cmdTable, char *command, void *data, char *args[MAX_BOT_ARGS]);

void command_cleanup(HashTable *cmdTable);

int command_reg_alias(HashTable *cmdTable, HashTable *cmdAliases, char *alias, char *cmd);

CmdAlias *command_alias_get(HashTable *cmdAliases, char *alias);

#endif //__COMMANDS_H__
