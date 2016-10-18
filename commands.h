#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include "globals.h"

typedef enum {
  CMDFLAG_MASTER = (1<<0),
} CommandFlags;

typedef struct BotCmd {
  int flags;
  char cmd[MAX_CMD_LEN];
  int args;
  int (*fn)(void *, char *a[MAX_BOT_ARGS]);
  struct BotCmd *next;
} BotCmd;

typedef int (*CommandFn)(void *, char *a[MAX_BOT_ARGS]);

extern BotCmd *command_global(void);

extern void command_reg_r(BotCmd **commands, char *cmdtag, int flags, int args, CommandFn fn);
extern void command_reg(char *cmdtag, int flags, int args, CommandFn fn);

extern BotCmd *command_get_r(BotCmd *commands, char *command);
extern BotCmd *command_get(char *command);

extern int command_call_r(BotCmd *commands, char *command, void *data, char *args[MAX_BOT_ARGS]);
extern int command_call(char *command, void *data, char *args[MAX_BOT_ARGS]);

#endif //__COMMANDS_H__
