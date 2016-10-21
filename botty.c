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

#define BOT_COUNT 1


IrcInfo server = {
  .port     = "6667",
  .server   = "CHANGETHIS",
  .channel  = "#CHANGETHIS"
};

BotInfo conInfo[2] = {
  {
    .info     = &server,
    .host     = "CIRCBotHost",
    .nick     = {"DiceBot", "CIrcBot2", "CIrcBot3"},
    .ident    = "CIrcBot",
    .realname = "Botty McBotFace",
    .master   = "Derrick",
  },
  {
    .info     = &server,
    .host     = "CIRCBotHost",
    .nick     = {"GunBot", "CIrcBot2", "CIrcBot3"},
    .ident    = "CIrcBot",
    .realname = "Botty McBotFace",
    .master   = "Derrick",
  }
};


/*
 * Callback functions can be used for adding
 * features or logic to notable  responses or events.
 */
int onConnect(void *data, IrcMsg *msg) {
  printf("BOT HAS CONNECTED!\n");
  return 0;
}

int onJoin(void *data, IrcMsg *msg) {
  botSend((BotInfo *)data, NULL, "Hello, World!");
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

int onNickChange(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  BotInfo *i = (BotInfo *)data;
  botSend(i, NULL, "I see what you did there %s... AKA %s!", msg->msg, msg->nick);
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

  botSend(data->bot, NULL, args[1]);
  return 0;
}

int botcmd_die(void *i, char *args[MAX_BOT_ARGS]) {
  printf("COMMAND RECEIVED: %s\n", args[0]);
  CmdData *data = (CmdData *)i;
  botSend(data->bot, NULL, "Seeya!");
  ircSend(data->bot->servfds.fd, "QUIT :leaving");
  return -1;
}

/* Hacky roulette game implementation */
int botcmd_roulette(void *i, char *args[MAX_BOT_ARGS]) {
  #define BULLETS 6
  #define QUOTE "You've got to ask yourself one question: \"do I feel lucky?\" Well do you punk?"
  //preserve game state across function calls
  typedef struct roulette {
    char state:2;
    unsigned char shot:3;
    unsigned char loop:2;
    unsigned char doQuote: 1;
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
      ctcpSend(data->bot, NULL, "ACTION", "loads a round then spins the chamber.");
      game.shot = (rand() % BULLETS + 1) + 1;
      game.doQuote = (game.shot >= BULLETS);
      game.state = 1;
      break;
    case 1:
      if (--game.shot == 0) {
        ctcpSend(data->bot, NULL, "ACTION", "BANG! %s is dead.", data->msg->nick);
        //reload the gun once it has been shot
        game.state = 0;
        game.loop++;
      } else
        ctcpSend(data->bot, NULL, "ACTION", "Click. %s is safe.", data->msg->nick);

      if (game.doQuote && game.shot == 2) botSend(data->bot, NULL, QUOTE);
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
    botSend(data->bot, NULL, "Missing dice information");
    return 0;
  }

  n = sscanf(args[1], "%u%c%u", &numDice, &delim, &dieMax);
  if (n < 3) {
    botSend(data->bot, NULL, "Invalid roll request: missing parameter");
    return 0;
  }
  else if (numDice > MAX_DICE || numDice < 1) {
    botSend(data->bot, NULL, "Invalid roll request: only 1 through 9 dice may be rolled.");
    return 0;
  }
  else if (dieMax < 2) {
    botSend(data->bot, NULL, "Invalid roll request: dice must have a max greater than 1");
    return 0;
  }

  int offset = snprintf(msg, MAX_MSG_LEN, "Rolled: ");
  for (int i = 0; i < numDice; i++) {
    int num = (rand() % dieMax) + 1;
    offset += snprintf(msg + offset, MAX_MSG_LEN, "%d ", num);
  }
  snprintf(msg + offset, MAX_MSG_LEN, "for %s", data->msg->nick);
  ctcpSend(data->bot, NULL, "ACTION", msg);
  return 0;
}


void printNick(NickList *n, void *data) {
  fprintf(stdout, "NICKDUMP: %s\n", n->nick);
}



int botcmd_dumpnames(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  bot_foreachName(data->bot, NULL, &printNick);
  return 0;
}

/*
 * Single bot with all functions
 */
int singlebot(int argc, char *argv[]) {
  int status = 0;
  
  //register some commands
  bot_addcommand(&conInfo[0], "say", 0, 2, &botcmd_say);
  bot_addcommand(&conInfo[0], "die", CMDFLAG_MASTER, 1, &botcmd_die);
  bot_addcommand(&conInfo[0],"roll", 0, 2, &botcmd_roll);
  bot_addcommand(&conInfo[0], "roulette", 0, 1, &botcmd_roulette);
  bot_addcommand(&conInfo[0], "nicks", 0, 1, &botcmd_dumpnames);
  
  bot_connect(&conInfo[0], argc, argv, 0);
  while (((status = bot_run(&conInfo[0])) >= 0)) {}
  bot_cleanup(&conInfo[0]);
  
  return status;
}

/*
 * Using multiple bots with different functions.
 */
int multibot(int argc, char *argv[]) {
  //register some commands
  //bot 1 has roll
  bot_addcommand(&conInfo[0],"roll", 0, 2, &botcmd_roll);
  //bot 2 has roulette
  bot_addcommand(&conInfo[1], "roulette", 0, 1, &botcmd_roulette);

  int status[BOT_COUNT] = {0}, exitsum = 0;
  for (int i = 0; i < BOT_COUNT; i++) {
    //default commands
    bot_addcommand(&conInfo[i], "say", 0, 2, &botcmd_say);
    bot_addcommand(&conInfo[i], "die", CMDFLAG_MASTER, 1, &botcmd_die);
    bot_connect(&conInfo[i], argc, argv, 0);
  }
  
  while (exitsum < BOT_COUNT) {
    for (int i = 0; i < BOT_COUNT; i++) {
      if (status[i] >= 0) {
        status[i] = bot_run(&conInfo[i]);
        exitsum += status[i] < 0;
      }
    }
  }
  for (int i = 0; i < BOT_COUNT; i++) bot_cleanup(&conInfo[i]);
  return 0;
}

int main(int argc, char *argv[]) {
  time_t t;
  srand((unsigned) time(&t));

  //hook in some callback functions
  callback_set(CALLBACK_CONNECT, &onConnect);
  callback_set(CALLBACK_JOIN, &onJoin);
  callback_set(CALLBACK_MSG, &onMsg);
  callback_set(CALLBACK_USRJOIN, &onUsrJoin);
  callback_set(CALLBACK_USRPART, &onUsrPart);
  callback_set(CALLBACK_SERVERCODE, &onServerResp);
  callback_set(CALLBACK_USRNICKCHANGE, &onNickChange);
  
  if (BOT_COUNT < 2) return singlebot(argc, argv);
  return multibot(argc, argv);
}

