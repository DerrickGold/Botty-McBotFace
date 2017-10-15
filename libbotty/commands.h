#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "globals.h"
#include "ircmsg.h"
#include "cmddata.h"

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
  int (*fn)(CmdData *, char *a[MAX_BOT_ARGS]);
} BotCmd;

typedef struct CmdAlias {
  BotCmd *cmd;
  char *args[MAX_BOT_ARGS];
  int argc;
  char *replaceWith;
} CmdAlias;

typedef int (*CommandFn)(CmdData *, char *a[MAX_BOT_ARGS]);

int commands_init(HashTable **commands);
int command_reg(HashTable *cmdTable, char *cmdtag, int flags, int args, CommandFn fn);
BotCmd *command_get(HashTable *cmdTable, char *command);
int command_call_r(BotCmd *cmd, CmdData *data, char *args[MAX_BOT_ARGS]);
int command_call(HashTable *cmdTable, char *command, CmdData *data, char *args[MAX_BOT_ARGS]);
void command_cleanup(HashTable **cmdTable);

int command_alias_init(HashTable **cmdaliases);
int command_reg_alias(HashTable *cmdTable, HashTable *cmdAliases, char *alias, char *cmd);
CmdAlias *command_alias_get(HashTable *cmdAliases, char *alias);
void command_alias_free(HashEntry *entry);
BotCmd *command_parse_ircmsg(IrcMsg *msg, HashTable *cmdTable, HashTable *cmdAliases);

#endif //__COMMANDS_H__
