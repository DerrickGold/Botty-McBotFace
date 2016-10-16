#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "commands.h"
#include "callback.h"
#include "ircmsg.h"
#include "connection.h"
#include "irc.h"

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
  printf("Recieved msg from %s in %s: %s", msg->nick, msg->channel, msg->msg);
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
 
/*
 * Some commands that the users can call.
 */
int botcmd_say(void *i, char *args[MAX_BOT_ARGS]) {
  printf("COMMAND RECEIVED: %s:\n", args[0]);
  for (int i = 0; i < MAX_BOT_ARGS; i++) {
    if (!args[i]) break;
    printf("ARG %d: %s\n", i, args[i]);
  }

  botSend((IrcInfo *)i, NULL, args[1]);
  return 0;
}

int botcmd_die(void *i, char *args[MAX_BOT_ARGS]) {
  printf("COMMAND RECEIVED: %s\n", args[0]);
  IrcInfo *info = (IrcInfo *)i;
  botSend(info, NULL, "Seeya!");
  ircSend(info->servfd, "QUIT :leaving");
  return -1;
}


int main(int argc, char *argv[]) {
  
  IrcInfo conInfo = {
    .host 		= "CIRCBotHost",
    .nick 		= "CIrcBot",
    .port 		= "6667",
    .ident 		= "CIrcBot",
    .realname = "Botty McBotFace",
    .master 	= "Derrick",
    .server 	= "CHANGETHIS",
    .channel 	= "#CHANGETHIS",
  };

  //hook in some callback functions
  callback_set(CALLBACK_CONNECT, &onConnect);
  callback_set(CALLBACK_JOIN, &onJoin);
  callback_set(CALLBACK_MSG, &onMsg);
  callback_set(CALLBACK_USRJOIN, &onUsrJoin);
  callback_set(CALLBACK_USRPART, &onUsrPart);
  
  //register some commands
  command_reg("say", 0, 2, &botcmd_say);
  command_reg("die", CMDFLAG_MASTER, 1, &botcmd_die);

  //run the bot
  return run(&conInfo, argc, argv, 0);
}

