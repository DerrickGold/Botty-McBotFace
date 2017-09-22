/*
 * A sample IRC bot using libbotty.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "botapi.h"

/*=====================================================
 * Bot Configuration
 *===================================================*/
BotInfo botInfo = {
  .info     = &(IrcInfo) {
    .port     = "6697",
    .server   = "CHANGE ME",
    .channel  = {"#CHANGEME", "\0", "\0", "\0", "\0"}
  },
  .host     = "CIRCBotHost",
  .nick     = {"DiceBot", "DrawBot", "CIrcBot3"},
  .ident    = "CIrcBot",
  .realname = "Botty McBotFace",
  .master   = "Derrick",
  .useSSL   = 1
};
/*=====================================================
 * Mailbox Structures and Methods
 *===================================================*/
#define MIN_MAIL_BOXES 13
/*
 * Each user can have a mailbox with
 * multiple messages stored for them to
 * view later.
 */
static HashTable *mailBoxes = NULL;

typedef struct Mail {
  time_t sent;
  char from[MAX_NICK_LEN];
  char msg[MAX_MSG_LEN];
  struct Mail *next;
} Mail;

typedef struct MailBox {
  char notified;
  int count;
  Mail *messages;
} MailBox;

char *getNotified(char *nick);
int numMsgs(char *nick);
void readMail(BotInfo *bot, char *respTarget, char *nick);
int saveMail(char *to, char *from, char *message);
void destroyAllMailBoxes(void);


static void mailNotify(BotInfo *bot, char *channel, char *nick) {
  //mail notification
  int left = numMsgs(nick);
  if (left > 0) {
     char *notStatus = getNotified(nick);
     if (!notStatus) return;
     if (!*notStatus) {
         botty_say(bot, channel, "%s: You have %d unread message(s). Use '~mail' to view them.", nick, left);
         *notStatus = 1;
     }
  }
}
/*=====================================================
 * Link History Structures and Methods
 *===================================================*/
#define LINKS_STORE_MAX 5
#define URL_IDENTIFIER_HTTP "http:"
#define URL_IDENTIFIER_HTTPS "https:"
#define URL_IDENTIFIER_WWW "www."

typedef struct LinkNode {
  char url[MAX_MSG_LEN];
  struct LinkNode *next;
} LinkNode;

typedef struct LinksHead {
  LinkNode *head;
  int count;
  LinkNode *lastPos;
} LinksHead;

LinksHead ListOfLinks = {};

char *links_msgContainsLink(char *input);
char links_store(LinksHead *head, char *input);
int links_print(CmdData *data, char *args[MAX_BOT_ARGS]);
void links_purge(LinksHead *list);

/*=====================================================
 * Bot Callback functions
 *===================================================*/
/*
 * Callback functions can be used for adding
 * features or logic to notable  responses or events.
 */
static int onConnect(void *data, IrcMsg *msg) {
  printf("BOT HAS CONNECTED!\n");
  return 0;
}

static int onJoin(void *data, IrcMsg *msg) {
  botty_say((BotInfo *)data, msg->channel, "Hello, World!");
  return 0;
}

static int onMsg(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;

  printf("Recieved msg from %s in %s: %s\n", msg->nick, msg->channel, msg->msg);
  mailNotify((BotInfo *)data, msg->channel,  msg->nick);
  links_store(&ListOfLinks, msg->msg);
  return 0;
}

static int onUsrJoin(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  printf("'%s' has joined the channel\n", msg->nick);
  mailNotify((BotInfo *)data, msg->channel, msg->nick);
  return 0;
}

static int onUsrPart(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  fprintf(stderr, "%s has left the channel\n", msg->nick);

  //reset mail notification
  char *notStatus = getNotified(msg->nick);
  if (notStatus) *notStatus = 0;
  return 0;
}

static int onNickChange(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;
  BotInfo *i = (BotInfo *)data;
  botty_say(i, msg->channel, "I see what you did there %s... AKA %s!", msg->msg, msg->nick);
  mailNotify((BotInfo *)data, msg->channel, msg->nick);
  return 0;
}

static int onUsrInvite(void *data, IrcMsg *msg) {
  botty_join((BotInfo *)data, msg->msg);
  return 0;
}

static int onServerResp(void *data, IrcMsg *msg) {
  if (!data || !msg) return -1;

  fprintf(stderr, "Received code: %s\n", msg->action);
  for (int i = 0; i < MAX_PARAMETERS; i++) {
    if (!msg->msgTok[i]) break;

    fprintf(stderr, "Parameter %d: %s\n", i, msg->msgTok[i]);
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

  if (!to || !msg) {
    botty_say(data->bot, responseTarget,
               "%s: Malformed mail command, must contain a destination nick and a message.",
               data->msg->nick);
    return 0;
  }

  if (saveMail(to, data->msg->nick, msg)) {
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

//mail command to read any inboxed messages
int botcmd_mail(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *responseTarget = botcmd_builtin_getTarget(data);
  readMail(data->bot, responseTarget, data->msg->nick);
  int left = numMsgs(data->msg->nick);
  botty_say(data->bot, responseTarget, "%s: You have %d message(s) remaining.", data->msg->nick, left);
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


static void printNick(NickList *n, void *data) {
  fprintf(stdout, "NICKDUMP: %s\n", n->nick);
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
static int _draw(void *b, BotProcessArgs *args) {
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
  fclose(input);
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





int main(int argc, char *argv[]) {
  int status = 0;
  time_t t;
  srand((unsigned) time(&t));

  //hook in some callback functions
  botty_setCallback(&botInfo, CALLBACK_CONNECT, &onConnect);
  botty_setCallback(&botInfo, CALLBACK_JOIN, &onJoin);
  botty_setCallback(&botInfo, CALLBACK_MSG, &onMsg);
  botty_setCallback(&botInfo, CALLBACK_USRJOIN, &onUsrJoin);
  botty_setCallback(&botInfo, CALLBACK_USRPART, &onUsrPart);
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

  botty_connect(&botInfo);

  //process input 30 times per second
  struct timespec sleepTimer = {
    .tv_sec = 0,
    .tv_nsec = ONE_SEC_IN_NS/120
  };
  while (((status = botty_process(&botInfo)) >= 0)) {
    //prevent 100% cpu usage
    nanosleep(&sleepTimer, NULL);
  }
  botty_cleanup(&botInfo);

  destroyAllMailBoxes();
  links_purge(&ListOfLinks);
  return status;
}

/*=====================================================
 * Mailbox implementation
 *
 *  HashTable functions are included in libbotty
 *===================================================*/
void cleanupMailBox(MailBox *box) {
  if (!box) return;

  Mail *cur = box->messages, *next = NULL;
  while (cur) {
    next = cur->next;
    free(cur);
    cur = next;
  }

  free(box);
}

static int cleanupHashedBox(HashEntry *entry, void *data) {
  //cleanup user name and stored messages
  if (entry->key) free(entry->key);
  cleanupMailBox((MailBox *)entry->data);
  return 0;
}

void destroyAllMailBoxes(void) {
  HashTable_forEach(mailBoxes, NULL, &cleanupHashedBox);
  HashTable_destroy(mailBoxes);
}

int saveMail(char *to, char *from, char *message) {
  //make sure our mail boxes exist
  if (!mailBoxes) {
    mailBoxes = HashTable_init(MIN_MAIL_BOXES);
    if (!mailBoxes) {
      fprintf(stderr, "ERROR ALLOCATING MAILBOXES\n");
      return -1;
    }
  }

  //make sure the user has an inbox
  HashEntry *user = HashTable_find(mailBoxes, to);
  if (!user) {
    fprintf(stdout, "'%s' does not have a box, creating one...\n", to);
    MailBox *newBox = calloc(1, sizeof(MailBox));
    if (!newBox) {
      fprintf(stderr, "error allocating message box for user %s\n", to);
      return -1;
    }

    char *nick = calloc(1, strlen(to) + 1);
    if (!nick) {
      fprintf(stderr, "error allocating nick for user's inbox\n");
      return -1;
    }
    strncpy(nick, to, strlen(to));

    HashEntry *newUser = HashEntry_create(nick, newBox);
    if (!newUser) {
      fprintf(stderr, "Error allocating mailbox for nick: %s\n", nick);
      free(nick);
      cleanupMailBox(newBox);
      return -1;
    }
    fprintf(stdout, "Adding '%s' to mailbox hash\n", nick);
    if (!HashTable_add(mailBoxes, newUser)) {
      fprintf(stderr, "Error adding %s's mail box to the hash\n", nick);
      cleanupHashedBox(newUser, NULL);
      return -1;
    }
    fprintf(stdout, "Searching hash for '%s''s box\n", nick);
    user = HashTable_find(mailBoxes, nick);
    if (!user) {
      fprintf(stderr, "Fatal error, could not retrieve created user (%s)\n", nick);
      return -1;
    }
    fprintf(stdout, "'%s''s box was found\n", nick);
  }

  //make the new mail message to store
  Mail *newMail = calloc(1, sizeof(Mail));
  if (!newMail) {
    fprintf(stderr, "error allocating new mail for nick %s\n", to);
    return -1;
  }
  time(&newMail->sent);
  strncpy(newMail->from, from, MAX_NICK_LEN);
  strncpy(newMail->msg, message, MAX_MSG_LEN);

  //now add the new message to the user's inbox
  MailBox *inbox = (MailBox *)user->data;
  if (inbox->messages == NULL) {
    inbox->count = 1;
    inbox->messages = newMail;
  } else {
    Mail *curMail = inbox->messages;
    while (curMail->next) curMail = curMail->next;
    curMail->next = newMail;
    inbox->messages++;
  }

  return 0;
}

char *getNotified(char *nick) {
  HashEntry *user = HashTable_find(mailBoxes, nick);
  if (!user) return NULL;

  MailBox *box = (MailBox *)user->data;
  if (!box) return NULL;

  return &box->notified;
}

//get the number of messages available for a user
int numMsgs(char *nick) {
  if (!mailBoxes) {
    fprintf(stderr, "NUMMSGS NO BOXES\n");
    return 0;
  }

  HashEntry *user = HashTable_find(mailBoxes, nick);
  if (!user) {
    fprintf(stderr, "NUMMSGS NO USER: '%s'\n", nick);
    return 0;
  }

  MailBox *box = (MailBox *)user->data;
  if (box->count <= 0 || !box->messages) {
    fprintf(stderr, "NUMMSGS NO MAIL\n");
    return 0;
  }

  return box->count;
}

void readMail(BotInfo *bot, char *respTarget, char *nick) {
  int msgCount = numMsgs(nick);
  if (!msgCount) return;

  //if msg count is > 0, then the user exists and has a box with messages
  HashEntry *user = HashTable_find(mailBoxes, nick);
  MailBox *box = (MailBox *)user->data;
  Mail *message = box->messages;
  box->messages = message->next;

  //get sent time
  char buff[20];
  struct tm * timeinfo = localtime (&message->sent);
  strftime(buff, sizeof(buff), "%b %d @ %H:%M", timeinfo);

  botty_say(bot, respTarget, "%s: [%s <%s>] %s", nick, buff, message->from, message->msg);
  free(message);
  box->count--;
}

/*=====================================================
 * Links implementation
 *
 *===================================================*/

char *links_msgContainsLink(char *input) {

  char *url = strstr(input, URL_IDENTIFIER_HTTP);
  if (url) return url;

  url = strstr(input, URL_IDENTIFIER_HTTPS);
  if (url) return url;

  url = strstr(input, URL_IDENTIFIER_WWW);
  return url;
}

char links_store(LinksHead *head, char *input) {
  char *start = links_msgContainsLink(input);
  if (!head || !start) return 0;

  char *end = start;
  while (*end != ' ' && *end != '\0' && *end != '\n' && *end != '\r') end++;

  LinkNode *newNode = NULL;
  if (head->count < LINKS_STORE_MAX) {
    newNode = calloc(1, sizeof(LinkNode));
  } else {
    newNode = head->head;
    LinkNode *prevNode = newNode;
    while (newNode->next){
      prevNode = newNode;
      newNode = newNode->next;
    }
    prevNode->next = NULL;
  }
  memset(newNode->url, 0, MAX_MSG_LEN);
  strncpy(newNode->url, start, (end - start));
  if (!head->head && head->count == 0) {
    head->head = newNode;
    head->count++;
  } else {
    newNode->next = head->head;
    head->head = newNode;
    head->count += (head->count < LINKS_STORE_MAX);
  }

  return 0;
}

int links_print_process(void *b, BotProcessArgs *args) {
  BotInfo *bot = (BotInfo *)b;
  LinksHead *listData = (LinksHead *)args->data;
  char *responseTarget = args->target;

  if (!listData->lastPos)
    goto _fin;

  if (botty_say(bot, responseTarget, ". %s", listData->lastPos->url) < 0)
    goto _fin;

  listData->lastPos = listData->lastPos->next;
  //return 1 to keep the process going
  return 1;

  _fin:
  return -1;
}

int links_print(CmdData *data, char *args[MAX_BOT_ARGS]) {
  char *caller = data->msg->nick;
  char *responseTarget = botty_respondDest(data);
  char *script = "links";

  if (ListOfLinks.count == 0) {
    botty_say(data->bot, responseTarget, "%s: There is no link history to post.", caller);
    return 0;
  }

  BotProcessArgs *sArgs = botty_makeProcessArgs((void*)&ListOfLinks, responseTarget, NULL);
  if (!sArgs) {
    botty_say(data->bot, responseTarget, "There was an error allocating memory to execute command: %s", script);
    return 0;
  }

  ListOfLinks.lastPos = ListOfLinks.head;
  botty_say(data->bot, responseTarget, "Printing the last %d available chat link(s) in history.", ListOfLinks.count);
  botty_runProcess(data->bot, &links_print_process, sArgs, script, caller);
  return 0;
}

void links_purge(LinksHead *list) {
  if (!list || !list->head) return;
  LinkNode *current = list->head;
  while (current->next) {
    LinkNode *next = current->next;
    free(current);
    current = next;
  }
  memset(list, 0, sizeof(LinksHead));
}
