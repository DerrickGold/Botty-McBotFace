#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <poll.h>

#include "globals.h"
#include "commands.h"
#include "callback.h"
#include "ircmsg.h"
#include "connection.h"
#include "irc.h"
#include "cmddata.h"

/*
 * Callback functions can be used for adding
 * features or logic to notable  responses or events.
 */
int onConnect(void *data, IrcMsg *msg) {
  printf("BOT HAS CONNECTED!\n");
  return 0;
}

int onJoin(void *data, IrcMsg *msg) {
  botSend((IrcInfo *)data, NULL, "Hello, World!");
  return 0;
}

int onMsg(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  
  printf("Recieved msg from %s in %s: %s\n", msg->nick, msg->channel, msg->msg);
  return 0;
}

int onUsrJoin(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  printf("%s has joined the channel\n", msg->nick);
  return 0;
}

int onUsrPart(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  printf("%s has left the channel\n", msg->nick);
  return 0;
}


int onServerResp(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;

  printf("Received code: %s\n", msg->action);
  for (int i = 0; i < MAX_PARAMETERS; i++) {
    if (!msg->msgTok[i]) break;

    printf("Parameter %d: %s\n", i, msg->msgTok[i]);
  }

  return 0;
}
 
/*
 * Some commands that the users can call.
 */
int botcmd_say(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  printf("COMMAND RECEIVED: %s:\n", args[0]);
  for (int i = 0; i < MAX_BOT_ARGS; i++) {
    if (!args[i]) break;
    printf("ARG %d: %s\n", i, args[i]);
  }

  botSend(data->info, NULL, args[1]);
  return 0;
}

int botcmd_die(void *i, char *args[MAX_BOT_ARGS]) {
  printf("COMMAND RECEIVED: %s\n", args[0]);
  CmdData *data = (CmdData *)i;
  botSend(data->info, NULL, "Seeya!");
  ircSend(data->info->servfds.fd, "QUIT :leaving");
  return -1;
}

/* Hacky roulette game implementation */
int botcmd_roulette(void *i, char *args[MAX_BOT_ARGS]) {
  #define BULLETS 6

  //preserve game state across function calls
  typedef struct roulette {
    char shot:3;
    char state:2;
    unsigned char loop:2;
  } roulette;

  static roulette game = {.shot = 0, .state = -1};
  CmdData *data = (CmdData *)i;
  game.loop = 0;
  
  do {
    switch (game.state) {
    default: {
      //first person to call roulette forces the gun to load
      //and then pulls the trigger on themselves.
      game.loop++;
    }
    case 0:
      ctcpSend(data->info, NULL, "ACTION", "loads a round then spins the chamber.");
      game.shot = (rand() % BULLETS) + 1;
      game.state = 1;
      break;
    case 1:
      if (--game.shot == 0) {
        ctcpSend(data->info, NULL, "ACTION", "BANG! %s is dead.", data->msg->nick);
        //reload the gun once it has been shot
        game.state = 0;
        game.loop++;
      } else
        ctcpSend(data->info, NULL, "ACTION", "Click. %s is safe.", data->msg->nick);
      break;
    }
  } while (game.loop--);
  
  return 0;
}

int botcmd_roll(void *i, char *args[MAX_BOT_ARGS]) {
  #define MAX_DICE 9
  
  CmdData *data = (CmdData *)i;
  char msg[MAX_MSG_LEN];
  int numDice = 0, dieMax = 0, n = 0;
  char delim = '\0';
  
  if (!args[1]) {
    botSend(data->info, NULL, "Missing dice information");
    return 0;
  }

  n = sscanf(args[1], "%u%c%u", &numDice, &delim, &dieMax);
  if (n < 3) {
    botSend(data->info, NULL, "Invalid roll request: missing parameter");
    return 0;
  }
  else if (numDice > MAX_DICE || numDice < 1) {
    botSend(data->info, NULL, "Invalid roll request: only 1 through 9 dice may be rolled.");
    return 0;
  }
  else if (dieMax < 2) {
    botSend(data->info, NULL, "Invalid roll request: dice must have a max greater than 1");
    return 0;
  }

  int offset = snprintf(msg, MAX_MSG_LEN, "Rolled: ");
  for (int i = 0; i < numDice; i++) {
    int num = (rand() % dieMax) + 1;
    offset += snprintf(msg + offset, MAX_MSG_LEN, "%d ", num);
  }
  snprintf(msg + offset, MAX_MSG_LEN, "for %s", data->msg->nick);
  ctcpSend(data->info, NULL, "ACTION", msg);
  return 0;
}




int main(int argc, char *argv[]) {
  int status = 0;
  time_t t;
  srand((unsigned) time(&t));
      
  IrcInfo conInfo = {
    .host 		= "CIRCBotHost",
    .nick 		= {"CIrcBot", "CIrcBot2", "CIrcBot3"},
    .port 		= "6667",
    .ident 		= "CIrcBot",
    .realname	= "Botty McBotFace",
    .master		= "Derrick",
    .server		= "awx.io",
    .channel	= "#bottester",
  };

  //hook in some callback functions
  callback_set(CALLBACK_CONNECT, &onConnect);
  callback_set(CALLBACK_JOIN, &onJoin);
  callback_set(CALLBACK_MSG, &onMsg);
  callback_set(CALLBACK_USRJOIN, &onUsrJoin);
  callback_set(CALLBACK_USRPART, &onUsrPart);
  callback_set(CALLBACK_SERVERCODE, &onServerResp);
  
  //register some commands
  command_reg("say", 0, 2, &botcmd_say);
  command_reg("die", CMDFLAG_MASTER, 1, &botcmd_die);
  command_reg("roulette", 0, 1, &botcmd_roulette);
  command_reg("roll", 0, 2, &botcmd_roll);
  
  bot_connect(&conInfo, argc, argv, 0);
  while (((status = bot_run(&conInfo)) >= 0)) {}
  bot_cleanup(&conInfo);
  
  return status;
}

