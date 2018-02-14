/*
 * A sample IRC bot using libbotty.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libgen.h>

#include "botapi.h"
#include "commands/mailbox.h"
#include "commands/links.h"

/*=====================================================
 * Bot Configuration
 *===================================================*/
BotInfo botInfo = {
  .info     = &(IrcInfo) {},
  .useSSL   = 1
};


/*=====================================================
 * Bot Callback functions
 *===================================================*/
/*
 * Callback functions can be used for adding
 * features or logic to notable  responses or events.
 */
static int onConnect(void *data, IrcMsg *msg) {
  syslog(LOG_INFO, "BOT HAS CONNECTED!");
  return 0;
}

static int onJoin(void *data, IrcMsg *msg) {
  botty_say((BotInfo *)data, msg->channel, "Hello, World!");
  return 0;
}

static int onMsg(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;

  syslog(LOG_DEBUG, "Recieved msg from %s in %s: %s", msg->nick, msg->channel, msg->msg);
  MailBox_notifyUser((BotInfo *)data, msg->channel,  msg->nick);
  links_store(msg->msg);
  return 0;
}

static int onUsrJoin(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  syslog(LOG_INFO, "'%s' has joined the channel", msg->nick);
  MailBox_notifyUser((BotInfo *)data, msg->channel, msg->nick);
  return 0;
}

static int onUsrPart(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  syslog(LOG_INFO, "%s has left the channel", msg->nick);
  MailBox_resetUserNotification(msg->nick);
  return 0;
}

static int onUsrQuit(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  syslog(LOG_INFO, "%s has disconnected the server", msg->nick);
  MailBox_resetUserNotification(msg->nick);
  return 0;
}

static int onNickChange(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  BotInfo *i = (BotInfo *)data;

  char *newNick = msg->msg;
  syslog(LOG_DEBUG, "OldNick: %s, NewNick: %s: Channel: %s", msg->nick, newNick, msg->channel);

  if (botty_msgContainsValidChannel(msg)) {
    botty_say(i, msg->channel, "I see what you did there %s... AKA %s!", newNick, msg->nick);
    MailBox_notifyUser((BotInfo *) data, msg->channel, newNick);
  }

  return 0;
}

static int onUsrInvite(void *data, IrcMsg *msg) {
  if (!strncmp(msg->nick, botInfo.master, MAX_NICK_LEN))
    botty_join((BotInfo *)data, msg->msg);

  return 0;
}

static int onServerResp(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;

  syslog(LOG_DEBUG, "Received code: %s", msg->action);
  for (int i = 0; i < MAX_PARAMETERS; i++) {
    if (!msg->msgTok[i]) break;

    syslog(LOG_DEBUG, "Parameter %d: %s", i, msg->msgTok[i]);
  }

  return 0;
}


/*=====================================================
 * Bot Command functions
 *===================================================*/

//message command for use with mailboxes
int botcmd_msg(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *responseTarget = botcmd_builtin_getTarget(data);
  char *to = args[1], *msg = args[2];

  //prevent messages to the bot itself
  if (!strncmp(to, bot_getNick(data->bot), MAX_NICK_LEN)) {
    botty_say(data->bot, responseTarget,
              "%s: Got your message, I'm always listening ;)",
              data->msg->nick);

    return 0;
  }

  if (!to || !msg) {
    botty_say(data->bot, responseTarget,
               "%s: Malformed mail command, must contain a destination nick and a message.",
               data->msg->nick);
    return 0;
  }

  if (MailBox_saveMsg(to, data->msg->nick, msg)) {
    botty_say(data->bot, responseTarget,
               "%s: There was an error saving your message, contact bot owner for assistance.",
               data->msg->nick);
    return 0;
  }

  botty_say(data->bot, responseTarget,
             "%s: Your message will be delivered to %s upon their return.",
             data->msg->nick, to);
  return 0;
}


/*
 * Some fun commands that aren't necessary, but illustrate
 * how to use this bot api.
 */
int botcmd_say(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *responseTarget = botcmd_builtin_getTarget(data);
  botty_say(data->bot, responseTarget, args[1]);
  return 0;
}

/* Hacky roulette game implementation */
int botcmd_roulette(CmdData *data, char *args[MAX_BOT_ARGS]) {
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
  char *responseTarget = botcmd_builtin_getTarget(data);

  game.loop = 0;
  do {
    switch (game.state) {
    default: {
      //first person to call roulette forces the gun to load
      //and then pulls the trigger on themselves.
      game.loop++;
    }
    case 0:
      botty_ctcpSend(data->bot, responseTarget, "ACTION", "loads a round and spins the cylinder.");
      game.shot = (rand() % BULLETS + 1) + 1;
      game.doQuote = (game.shot >= BULLETS);
      game.state = 1;
      break;
    case 1:
      if (--game.shot == 0) {
        botty_ctcpSend(data->bot, responseTarget, "ACTION", "BANG! %s is dead.", data->msg->nick);
        //reload the gun once it has been shot
        game.state = 0;
        game.loop++;
      } else
        botty_ctcpSend(data->bot, responseTarget, "ACTION", "Click... %s is safe.", data->msg->nick);

      if (game.doQuote && game.shot == 2) botty_say(data->bot, responseTarget, QUOTE);
      break;
    }
  } while (game.loop--);

  return 0;
}

int botcmd_roll(CmdData *data, char *args[MAX_BOT_ARGS]) {
  #define MAX_DICE 9

  char *responseTarget = botcmd_builtin_getTarget(data);
  char msg[MAX_MSG_LEN];
  int numDice = 0, dieMax = 0, n = 0;
  char delim = '\0';

  if (!args[1]) {
    botty_say(data->bot, responseTarget, "Missing dice information");
    return 0;
  }

  n = sscanf(args[1], "%u%c%u", &numDice, &delim, &dieMax);
  if (n < 3) {
    botty_say(data->bot, responseTarget, "Invalid roll request: missing parameter");
    return 0;
  }
  else if (numDice > MAX_DICE || numDice < 1) {
    botty_say(data->bot, responseTarget, "Invalid roll request: only 1 through 9 dice may be rolled.");
    return 0;
  }
  else if (dieMax < 2) {
    botty_say(data->bot, responseTarget, "Invalid roll request: dice must have a max greater than 1");
    return 0;
  }

  int offset = snprintf(msg, MAX_MSG_LEN, "Rolled: ");
  for (int i = 0; i < numDice; i++) {
    int num = (rand() % dieMax) + 1;
    offset += snprintf(msg + offset, MAX_MSG_LEN, "%d ", num);
  }
  snprintf(msg + offset, MAX_MSG_LEN, "for %s", data->msg->nick);
  botty_ctcpSend(data->bot, responseTarget, "ACTION", msg);
  return 0;
}


static void printNick(NickListEntry *n, void *data) {
  syslog(LOG_INFO, "NICKDUMP: %s", n->nick);
}

int botcmd_dumpnames(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *responseTarget = botcmd_builtin_getTarget(data);
  bot_foreachName(data->bot, responseTarget, NULL, &printNick);
  return 0;
}


static int _draw_free(void *a) {
  FILE *fh = (FILE *)a;
  fclose(fh);
  return 0;
}

//A sample 'process' function that can be given to the bot
static int _draw(void *b, char *procOwner, BotProcessArgs *args) {
  BotInfo *bot = (BotInfo *)b;
  FILE *input = (FILE *)args->data;
  char *responseTarget = args->target;
  char buf[MAX_MSG_LEN];

  if (feof(input))
    goto _fin;

  char *s = fgets(buf, MAX_MSG_LEN, input);
  if (!s)
    goto _fin;

  char *newline = strchr(s, '\n');
  if (newline) *newline = '\0';

  if (botty_say(bot, responseTarget, ". %s", s) < 0)
    goto _fin;

  //return 1 to keep the process going
  return 1;

  _fin:
  //return negative value to indicate the process
  //is complete
  _draw_free(args->data);
  return -1;
}

int botcmd_draw(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *caller = data->msg->nick;
  char *responseTarget = botty_respondDest(data);
  char *script = "draw";

  char path[512];
  char *file = args[1];
  if (!file) {
    botty_say(data->bot, responseTarget, "%s: please specify a picture.", caller);
    return 0;
  }

  snprintf(path, sizeof(path), "art/%s.txt", file);
  FILE *f = fopen(path, "rb");
  if (!f) {
    botty_say(data->bot, responseTarget, "File '%s' does not exist!", file);
    return 0;
  }

  BotProcessArgs *sArgs = botty_makeProcessArgs((void *)f, responseTarget, &_draw_free);
  if (!sArgs) {
    botty_say(data->bot, responseTarget, "There was an error allocating memory to execute command: %s", script);
    fclose(f);
    return 0;
  }

  //initialize and start the draw process
  //A process will block all input for a given bot until
  //it has completed the assigned process.
  botty_runProcess(data->bot, &_draw, sArgs, script, caller);
  return 0;
}

int test_notice(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *caller = data->msg->nick;
  char *responseTarget = caller;
  char *msg = args[1];

  botty_send(data->bot, responseTarget, NOTICE_ACTION, NULL, "%s", msg);
  return 0;
}




int main(int argc, char *argv[]) {
  char configPath[MAX_FILEPATH_LEN];
  char *cfgPtr = configPath;

  openlog(argv[0], LOG_PERROR | LOG_CONS | LOG_PID, LOG_SYSLOG);

  if (argc < 2) {
    realpath(argv[0], configPath);
    char *config = dirname(configPath);
    snprintf(configPath, MAX_FILEPATH_LEN - 1, "%s/%s", config, DEFAULT_CONFIG_FILE);
    syslog(LOG_NOTICE, "Loading default config file: %s", cfgPtr);
  }
  else {
    cfgPtr = argv[1];
    syslog(LOG_NOTICE, "Loading user specified config: %s", cfgPtr);
  }

  if (botty_loadConfig(&botInfo, cfgPtr)) {
    syslog(LOG_CRIT, "Error loading config file: %s", cfgPtr);
    return -1;
  }


  int status = 0;
  time_t t;
  srand((unsigned) time(&t));

  //hook in some callback functions
  botty_setCallback(&botInfo, CALLBACK_CONNECT, &onConnect);
  botty_setCallback(&botInfo, CALLBACK_JOIN, &onJoin);
  botty_setCallback(&botInfo, CALLBACK_MSG, &onMsg);
  botty_setCallback(&botInfo, CALLBACK_USRJOIN, &onUsrJoin);
  botty_setCallback(&botInfo, CALLBACK_USRPART, &onUsrPart);
  botty_setCallback(&botInfo, CALLBACK_USRQUIT, &onUsrQuit);
  botty_setCallback(&botInfo, CALLBACK_SERVERCODE, &onServerResp);
  botty_setCallback(&botInfo, CALLBACK_USRNICKCHANGE, &onNickChange);
  botty_setCallback(&botInfo, CALLBACK_USRINVITE, &onUsrInvite);

  if (botty_init(&botInfo, argc, argv, 0))
    return -1;

  //register some extra commands
  botty_addCommand(&botInfo, "say", 0, 2, &botcmd_say);
  botty_addCommand(&botInfo,"roll", 0, 2, &botcmd_roll);
  botty_addCommand(&botInfo, "roulette", 0, 1, &botcmd_roulette);
  botty_addCommand(&botInfo, "nicks", 0, 1, &botcmd_dumpnames);

  botty_addCommand(&botInfo, "msg", 0, 3, &botcmd_msg);
  botty_addCommand(&botInfo, "mail", 0, 1, &botcmd_mail);
  botty_addCommand(&botInfo, "draw", 0, 2, &botcmd_draw);
  botty_addCommand(&botInfo, "links", 0, 1, &links_print);
  botty_addCommand(&botInfo, "whisper", 0, 2, &test_notice);

  //start the bot connection to the irc server
  botty_connect(&botInfo);

  while (((status = botty_process(&botInfo)) >= 0)) {
    //prevent 100% cpu usage
    nanosleep(&(struct timespec) {
      .tv_sec = 0,
      .tv_nsec = ONE_SEC_IN_NS/120
    }, NULL);
  }

  botty_cleanup(&botInfo);
  MailBox_destroyAll();
  links_purge();

  closelog();
  return status;
}
