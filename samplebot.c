/*
 * A sample IRC bot using libbotty.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "botapi.h"

BotInfo conInfo = {
  .info     = &(IrcInfo) {
    .port     = "6667",
    .server   = "awx.io",
    .channel  = "#bottester"
  },
  .host     = "CIRCBotHost",
  .nick     = {"DiceBot", "CIrcBot2", "CIrcBot3"},
  .ident    = "CIrcBot",
  .realname = "Botty McBotFace",
  .master   = "Derrick",
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
  botty_send((BotInfo *)data, NULL, "Hello, World!");
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
  botty_send(i, NULL, "I see what you did there %s... AKA %s!", msg->msg, msg->nick);
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
 * Some fun commands that aren't necessary, but illustrate 
 * how to use this bot api.
 */
int botcmd_say(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  botty_send(data->bot, NULL, args[1]);
  return 0;
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
      botty_ctcpSend(data->bot, NULL, "ACTION", "loads a round then spins the chamber.");
      game.shot = (rand() % BULLETS + 1) + 1;
      game.doQuote = (game.shot >= BULLETS);
      game.state = 1;
      break;
    case 1:
      if (--game.shot == 0) {
        botty_ctcpSend(data->bot, NULL, "ACTION", "BANG! %s is dead.", data->msg->nick);
        //reload the gun once it has been shot
        game.state = 0;
        game.loop++;
      } else
        botty_ctcpSend(data->bot, NULL, "ACTION", "Click. %s is safe.", data->msg->nick);

      if (game.doQuote && game.shot == 2) botty_send(data->bot, NULL, QUOTE);
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
    botty_send(data->bot, NULL, "Missing dice information");
    return 0;
  }

  n = sscanf(args[1], "%u%c%u", &numDice, &delim, &dieMax);
  if (n < 3) {
    botty_send(data->bot, NULL, "Invalid roll request: missing parameter");
    return 0;
  }
  else if (numDice > MAX_DICE || numDice < 1) {
    botty_send(data->bot, NULL, "Invalid roll request: only 1 through 9 dice may be rolled.");
    return 0;
  }
  else if (dieMax < 2) {
    botty_send(data->bot, NULL, "Invalid roll request: dice must have a max greater than 1");
    return 0;
  }

  int offset = snprintf(msg, MAX_MSG_LEN, "Rolled: ");
  for (int i = 0; i < numDice; i++) {
    int num = (rand() % dieMax) + 1;
    offset += snprintf(msg + offset, MAX_MSG_LEN, "%d ", num);
  }
  snprintf(msg + offset, MAX_MSG_LEN, "for %s", data->msg->nick);
  botty_ctcpSend(data->bot, NULL, "ACTION", msg);
  return 0;
}


static void printNick(NickList *n, void *data) {
  fprintf(stdout, "NICKDUMP: %s\n", n->nick);
}

int botcmd_dumpnames(void *i, char *args[MAX_BOT_ARGS]) {
  CmdData *data = (CmdData *)i;
  bot_foreachName(data->bot, NULL, &printNick);
  return 0;
}


int main(int argc, char *argv[]) {
  int status = 0;
  time_t t;
  srand((unsigned) time(&t));

  //hook in some callback functions
  botty_setCallback(&conInfo, CALLBACK_CONNECT, &onConnect);
  botty_setCallback(&conInfo, CALLBACK_JOIN, &onJoin);
  botty_setCallback(&conInfo, CALLBACK_MSG, &onMsg);
  botty_setCallback(&conInfo, CALLBACK_USRJOIN, &onUsrJoin);
  botty_setCallback(&conInfo, CALLBACK_USRPART, &onUsrPart);
  botty_setCallback(&conInfo, CALLBACK_SERVERCODE, &onServerResp);
  botty_setCallback(&conInfo, CALLBACK_USRNICKCHANGE, &onNickChange);

  if (botty_init(&conInfo, argc, argv, 0))
    return -1;
  
  //register some extra commands
  botty_addCommand(&conInfo, "say", 0, 2, &botcmd_say);
  botty_addCommand(&conInfo,"roll", 0, 2, &botcmd_roll);
  botty_addCommand(&conInfo, "roulette", 0, 1, &botcmd_roulette);
  botty_addCommand(&conInfo, "nicks", 0, 1, &botcmd_dumpnames);
  
  botty_connect(&conInfo);
  while (((status = botty_process(&conInfo)) >= 0)) {}
  botty_cleanup(&conInfo);
  
  return status;
}

