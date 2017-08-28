#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "builtin.h"
#include "irc.h"
#include "ircmsg.h"
#include "commands.h"
#include "callback.h"
#include "connection.h"
#include "cmddata.h"
#include "botmsgqueues.h"

#define QUEUE_SEND_MSG 1

HashTable *IrcApiActions = NULL;

const char IrcApiActionText[API_ACTION_COUNT][MAX_CMD_LEN] = {
  "NOP", "DIE", "WHO", "KICK", "NICK", "MODE", "INFO", "KILL",
  "PING", "TIME", "JOIN", "AWAY", "MOTD", "PONG", "OPER",
  "PART", "ISON", "LIST", "USER", "QUIT", "ADMIN", "TRACE",
  "NAMES", "TOPIC", "LINKS", "ERROR", "WHOIS", "STATS",
  "USERS", "SQUIT", "REHASH", "INVITE", "WHOWAS", "LUSERS",
  "SUMMON", "SQUERY", "CONNECT", "SERVICE", "WALLOPS", "RESTART",
  "VERSION", "SERVLIST", "USERHOST"
};

static IRC_API_Actions IrcApiActionValues[API_ACTION_COUNT];
int bot_parse(BotInfo *bot, char *line);

static int _processHashedMsgQueue(HashEntry *queueHashEntry, void *data) {
  BotInfo *bot = (BotInfo *)data;
  BotSendMessageQueue *queuedMessages = (BotSendMessageQueue *)queueHashEntry->data;
  BotMsgQueue_processQueue(&bot->conInfo, queuedMessages);
  return 0;
}

static void processMsgQueueHash(BotInfo *bot) {
  HashTable_forEach(bot->msgQueues, (void *)bot, *_processHashedMsgQueue);
}

/*
 * Send an irc formatted message to the server.
 * Assumes your message is appropriately sized for a single
 * message.
 */
static int _send(BotInfo *bot, char *command, char *target, char *msg, char *ctcp, char queued) {
  SSLConInfo *conInfo = &bot->conInfo;
  char curSendBuf[MAX_MSG_LEN];
  int written = 0;
  char *sep = PARAM_DELIM_STR;

  if (!command || !target) sep = ACTION_EMPTY;
  if (!command) command = ACTION_EMPTY;
  if (!target) target = ACTION_EMPTY;


  if (!ctcp)
    written = snprintf(curSendBuf, MAX_MSG_LEN, "%s %s %s%s%s", command, target, sep, msg, MSG_FOOTER);
  else {
    written = snprintf(curSendBuf, MAX_MSG_LEN, "%s %s %s"CTCP_MARKER"%s %s"CTCP_MARKER"%s",
                       command, target, sep, ctcp, msg, MSG_FOOTER);
  }

  if (queued) {
    BotQueuedMessage *toSend = BotQueuedMsg_newMsg(curSendBuf, target, written, bot->procQueue.curPid);
    if (toSend) BotMsgQueue_enqueueTargetMsg(bot->msgQueues, target, toSend);
    else fprintf(stderr, "Failed to queue message: %s\n", curSendBuf);
    return 0;
  }

  fprintf(stdout, "SENDING (%d bytes): %s\n", written, curSendBuf);
  return connection_client_send(conInfo, curSendBuf, written);
}

/*
 * Returns the number of bytes of overhead that is sent with each message.
 * This overhead goes towards the IRC message limit.
 */
static unsigned int _getMsgOverHeadLen(char *command, char *target, char *ctcp, char *usernick) {
  unsigned int len =  MAX_CMD_LEN + ARG_DELIM_LEN + MAX_CHAN_LEN + ARG_DELIM_LEN;
  len += strlen(MSG_FOOTER) + strlen(usernick);
  if (ctcp)
    len += strlen(ctcp) + (strlen(CTCP_MARKER) << 1) + ARG_DELIM_LEN;

  return len;
}

/*
 * Split and send an irc formatted message to the server.
 * If your message is too long, it will be split up and sent in up to
 * MAX_MSG_SPLITS chunks.
 *
 */
int bot_irc_send_s(BotInfo *bot, char *command, char *target, char *msg, char *ctcp, char *nick) {
  unsigned int overHead = _getMsgOverHeadLen(command, target, ctcp, nick);
  unsigned int msgLen =  overHead + strlen(msg);
  if (msgLen >= MAX_MSG_LEN) {
    //split the message into chunks
    int maxSplitSize = MAX_MSG_LEN - overHead;
    int chunks = (strlen(msg) / maxSplitSize) + 1;
    chunks = (chunks < MAX_MSG_SPLITS) ? chunks : MAX_MSG_SPLITS;

    char replaced = 0;
    char *nextMsg = msg, *end = 0, *last = msg + strlen(msg);
    do {
      end = nextMsg + maxSplitSize;
      //split on words, so scan back for last space
      while (*end != ' ' && end > nextMsg) end--;
      //if we couldn't find a word to split on, so just split at the character limit
      if (end <= nextMsg)  end = nextMsg + maxSplitSize;
      if (end < last) {
        replaced = *end;
        *end = '\0';
      }
      _send(bot, command, target, nextMsg, ctcp, QUEUE_SEND_MSG);
      nextMsg = end;
      if (end < last) *end = replaced;
      //remove any leading spaces for the next message
      if (*nextMsg == ' ') nextMsg++;
    } while (--chunks && nextMsg < last);

    return 0;
  }

  return _send(bot, command, target, msg, ctcp, QUEUE_SEND_MSG);
}

int bot_irc_send(BotInfo *bot, char *msg) {
  return _send(bot, NULL, NULL, msg, NULL, 0);
}


/*
 * Automatically formats a PRIVMSG command for the bot to speak.
 */

static int _botSend(BotInfo *bot, char *target, char *action, char *ctcp, char *fmt, va_list a) {
  char *msgBuf;
  if (!target) {
    //target = bot->info->channel;
    fprintf(stderr, "_botSend: No response target provided!\n");
    return 0;
  }
    

  //only buffer up to 4 message splits worth of text
  size_t msgBufLen = MAX_MSG_LEN * MAX_MSG_SPLITS;
  msgBuf = malloc(msgBufLen);
  if (!msgBuf) {
    perror("Msg alloc failed:");
    return -1;
  }
  vsnprintf(msgBuf, msgBufLen - 1, fmt, a);
  int status = bot_irc_send_s(bot, action, target, msgBuf, ctcp, bot_getNick(bot));
  free(msgBuf);
  return status;
}

int bot_send(BotInfo *bot, char *target, char *action, char *ctcp, char *fmt, ...) {
  int status = 0;
  va_list args;
  va_start(args, fmt);
  status = _botSend(bot, target, action, ctcp, fmt, args);
  va_end(args);
  return status;
}

int bot_ctcp_send(BotInfo *bot, char *target, char *command, char *msg, ...) {
  char outbuf[MAX_MSG_LEN];
  va_list args;
  va_start(args, msg);
  vsnprintf(outbuf, MAX_MSG_LEN, msg, args);
  va_end(args);
  return bot_send(bot, target, ACTION_MSG, NULL, CTCP_MARKER"%s %s"CTCP_MARKER, command, outbuf);
}



static int findThrottleTarget(HashEntry *queueHashEntry, void *data) {
  if (strlen(queueHashEntry->key) == 0)
    return 0;

  char *serverMessage = (char *)data;
  char *match = strstr(serverMessage, queueHashEntry->key);
  if (match) {
    BotSendMessageQueue *sendQueue = (BotSendMessageQueue *)queueHashEntry->data;
    sendQueue->throttled++;
    fprintf(stderr, "Detected throttling from: %s\n", queueHashEntry->key);
    return 1;
  }
  return 0;
}

static int handleMessageThrottling(BotInfo *bot, char *serverMessage) {
  char *result = strstr(serverMessage, THROTTLE_NEEDLE);
  if (!result) return 0;
  return HashTable_forEach(bot->msgQueues, (void *)serverMessage, &findThrottleTarget);
}

/*
 * Default actions for handling various server responses such as nick collisions
 * or throttling
 */
static int defaultServActions(BotInfo *bot, IrcMsg *msg, char *line) {
  //if nick is already registered, try a new one
  if (!strncmp(msg->action, REG_ERR_CODE, strlen(REG_ERR_CODE))) {
    if (bot->nickAttempt < NICK_ATTEMPTS) bot->nickAttempt++;
    else {
      fprintf(stderr, "Exhuasted nick attempts, please configure a unique nick\n");
      return -1;
    }
    fprintf(stderr, "Nick is already in use, attempting to use: %s\n", bot->nick[bot->nickAttempt]);
    //then attempt registration again
    bot->state = CONSTATE_CONNECTED;
    //return bot_parse(bot, line);
    return 0;
  }
  //otherwise, nick is not in use
  else if (!strncmp(msg->action, REG_SUC_CODE, strlen(REG_SUC_CODE))) {
    bot->state = CONSTATE_REGISTERED;
  }
  //store all current users in the channel
  else if (!strncmp(msg->action, NAME_REPLY, strlen(NAME_REPLY))) {
    char *start = msg->msgTok[1], *next = start, *end = start + strlen(start);
    while (start < end) {
      while (*next != BOT_ARG_DELIM && next <= end) next++;
      *next = '\0';
      bot_regName(bot, start);
      fprintf(stdout, "Registered nick: %s\n", start);
      if (next < end) {
        *next = BOT_ARG_DELIM;
        next++;
      }
      start = next;
    }
  }
  //attempt to detect any messages indicating throttling
  else if (!strncmp(msg->action, NOTICE_ACTION, strlen(NOTICE_ACTION))) {
    return handleMessageThrottling(bot, msg->msgTok[0]);
  }

  return 0;
}


/*
 * Parse out any server responses that may need to be attended to
 * and pass them into the appropriate callbacks.
 */
static int parseServer(BotInfo *bot, char *line) {
  char buf[MAX_MSG_LEN];
  snprintf(buf, sizeof(buf), ":%s", bot->info->server);
  //not a server response
  if (strncmp(line, buf, strlen(buf))) return 0;
  //is a server response
  strncpy(buf, line, MAX_MSG_LEN);
  IrcMsg *msg = ircMsg_server_new(buf);
  int status = defaultServActions(bot, msg, line);
  if (status) {
    free(msg);
    return status;
  }

  callback_call_r(bot->cb, CALLBACK_SERVERCODE, (void *)bot, msg);
  free(msg);
  return 1;
}

static int userJoined(BotInfo *bot, IrcMsg *msg) {
  bot_regName(bot, msg->nick);
  return callback_call_r(bot->cb, CALLBACK_USRJOIN, (void *)bot, msg);
}

static int userLeft(BotInfo *bot, IrcMsg *msg) {
  bot_rmName(bot, msg->nick);
  return callback_call_r(bot->cb, CALLBACK_USRPART, (void *)bot, msg);
}

static int userNickChange(BotInfo *bot, IrcMsg *msg) {
  bot_rmName(bot, msg->nick);
  bot_regName(bot, msg->msg);
  return callback_call_r(bot->cb, CALLBACK_USRNICKCHANGE, (void *)bot, msg);
}

/*
 * Parses any incomming line from the irc server and
 * invokes callbacks depending on the message type and
 * current state of the connection.
 */
int bot_parse(BotInfo *bot, char *line) {
  if (!line) return 0;

  int servStat = 0;
  char sysBuf[MAX_MSG_LEN];
  char *space = NULL, *space_off = NULL;
  fprintf(stdout, "SERVER: %s\n", line);

  //respond to server pings
  if (!strncmp(line, PING_STR, strlen(PING_STR))) {
    //find the start of the token we are supposed to pong with
    char *pongTok = line + strlen(PING_STR) + 1;
    snprintf(sysBuf, sizeof(sysBuf), PONG_STR" %s", pongTok);
    bot_irc_send(bot, sysBuf);
    return 0;
  }

  if ((servStat = parseServer(bot, line)) < 0) return servStat;

  switch (bot->state) {
  case CONSTATE_NONE:
    //initialize data here
    space = strtok_r(line, " ", &space_off);
    if (space) {
      //grab new server name if we've been redirected
      memcpy(bot->info->server, space+1, strlen(space) - 1);
      fprintf(stderr, "given server: %s\n", bot->info->server);
    }
    callback_call_r(bot->cb, CALLBACK_CONNECT, (void*)bot, NULL);
    bot->state = CONSTATE_CONNECTED;
    break;

  case CONSTATE_CONNECTED:
    //register the bot
    snprintf(sysBuf, sizeof(sysBuf), NICK_CMD_STR" %s", bot->nick[bot->nickAttempt]);
    bot_irc_send(bot, sysBuf);
    snprintf(sysBuf, sizeof(sysBuf), USER_CMD_STR" %s %s test: %s", bot->ident, bot->host, bot->realname);
    bot_irc_send(bot, sysBuf);
    //go to listening state to wait for registration confirmation
    bot->state = CONSTATE_LISTENING;
    break;

  case CONSTATE_REGISTERED:
    for (int i = 0; i < MAX_CONNECTED_CHANS; i++) {
      char *chan = bot->info->channel[i];
      if (*chan == '\0')
        break;
      bot_join(bot, chan);
    }
    bot->joined = 1;
    bot->state = CONSTATE_LISTENING;
    break;
  default:
  case CONSTATE_LISTENING:
    //filter out server messages
    if (servStat) break;

    snprintf(sysBuf, sizeof(sysBuf), ":%s", bot->nick[bot->nickAttempt]);
    if (!strncmp(line, sysBuf, strlen(sysBuf))) {
      //filter out messages that the bot says itself
      break;
    }
    else {
      BotCmd *cmd = NULL;
      IrcMsg *msg = ircMsg_irc_new(line, bot->commands, bot->cmdAliases, &cmd);
      IRC_API_Actions action = IRC_ACTION_NOP;
      HashEntry *a = HashTable_find(IrcApiActions, msg->action);

      if (cmd) {
        CmdData data = { .bot = bot, .msg = msg };
        //make sure who ever is calling the command has permission to do so
        if (cmd->flags & CMDFLAG_MASTER && strcmp(msg->nick, bot->master))
          fprintf(stderr, "%s is not %s\n", msg->nick, bot->master);
        else if ((servStat = command_call_r(cmd, (void *)&data, msg->msgTok)) < 0)
          fprintf(stderr, "Command '%s' gave exit code\n,", cmd->cmd);
      }
      else if (a) {
        if (a->data) action = *(IRC_API_Actions*)a->data;

        switch(action) {
        default: break;
        case IRC_ACTION_JOIN:
          servStat = userJoined(bot, msg);
          break;
        case IRC_ACTION_QUIT:
        case IRC_ACTION_PART:
          servStat = userLeft(bot, msg);
          break;
        case IRC_ACTION_NICK:
          servStat = userNickChange(bot, msg);
          break;
        }
      }
      else
        callback_call_r(bot->cb, CALLBACK_MSG, (void*)bot, msg);

      free(msg);
    }
    break;
  }
  return servStat;
}

/*
 * initialize the hash table used for looking up api calls
 */
int bot_irc_init(void) {
  if (IrcApiActions) return 0;

  IrcApiActions = HashTable_init(ACTION_HASH_SIZE);
  if (!IrcApiActions) {
    fprintf(stderr, "Error initializing IRC API hash\n");
    return -1;
  }

  for (int i = 0; i < API_ACTION_COUNT; i++) {
    IrcApiActionValues[i] = (IRC_API_Actions) i;
    HashTable_add(IrcApiActions,
                  HashEntry_create((char *)IrcApiActionText[i], (void *)&IrcApiActionValues[i]));
  }

  return 0;
}

void bot_irc_cleanup(void) {
  HashTable_destroy(IrcApiActions);
  IrcApiActions = NULL;
}

int bot_init(BotInfo *bot, int argc, char *argv[], int argstart) {
  if (!bot) return -1;

  bot->commands = HashTable_init(COMMAND_HASH_SIZE);
  if (!bot->commands) {
    fprintf(stderr, "Error allocating command hash for bot\n");
    return -1;
  }
  //initialize the built in commands
  botcmd_builtin(bot);

  bot->msgQueues = HashTable_init(QUEUE_HASH_SIZE);
  if (!bot->msgQueues) {
    fprintf(stderr, "Error initializing Bot Message Queue hash\n");
    return -1;
  }

  bot->cmdAliases = HashTable_init(ALIAS_HASH_SIZE);
  if (!bot->cmdAliases) {
    fprintf(stderr, "Error initialize bot cmd alias hash\n");
    return -1;
  }

  BotInputQueue_initQueue(&bot->inputQueue);
  return 0;
}

int bot_connect(BotInfo *bot) {
  if (!bot) return -1;

  bot->state = CONSTATE_NONE;

  if (bot->useSSL) {
    if (connection_ssl_client_init(bot->info->server, bot->info->port, &bot->conInfo))
      exit(1);

    return 0;
  }

  bot->conInfo.servfds.fd = connection_client_init(bot->info->server, bot->info->port, &bot->conInfo.res);
  if (bot->conInfo.servfds.fd < 0) exit(1);
  bot->conInfo.servfds.events = POLLIN | POLLPRI | POLLOUT | POLLWRBAND;
  return 0;
}

char *bot_getNick(BotInfo *bot) {
  return bot->nick[bot->nickAttempt];
}

void bot_cleanup(BotInfo *bot) {
  if (!bot) return;

  BotProcess_freeProcesaQueue(&bot->procQueue);

  bot_purgeNames(bot);
  if (bot->commands) command_cleanup(bot->commands);
  bot->commands = NULL;
  if (bot->msgQueues) BotMsgQueue_cleanQueues(bot->msgQueues);
  bot->msgQueues = NULL;

  BotInputQueue_clearQueue(&bot->inputQueue);
  
  close(bot->conInfo.servfds.fd);
  freeaddrinfo(bot->conInfo.res);
}

void bot_setCallback(BotInfo *bot, BotCallbackID id, Callback fn) {
  callback_set_r(bot->cb, id, fn);
}

void bot_addcommand(BotInfo *bot, char *cmd, int flags, int args, CommandFn fn) {
  command_reg(bot->commands, cmd, flags, args, fn);
}


/*
 * Run the bot! The bot will connect to the server and start
 * parsing replies.
 */
int bot_run(BotInfo *bot) {
  int n = 0, ret = 0;

  if (!bot->joined && bot->state == CONSTATE_LISTENING) {
    TimeStamp_t currentTime = botty_currentTimestamp();
    if (bot->startTime == 0) bot->startTime = botty_currentTimestamp();
    else if(currentTime - bot->startTime >= REGISTER_TIMEOUT_SEC * ONE_SEC_IN_MS) {
      bot->state = CONSTATE_REGISTERED;
    }
  }

  //read from wire
  memset(bot->recvbuf, 0, sizeof(bot->recvbuf));
  if (connection_client_poll(&bot->conInfo, POLLIN, &ret)) {
    n = connection_client_read(&bot->conInfo, bot->recvbuf, sizeof(bot->recvbuf));
    if (!n) {
      fprintf(stderr, "Remote closed connection\n");
      return -2;
    }
    else if (!bot->conInfo.enableSSL && n < 0) {
      perror("Response error: ");
      return -3;
    }
  }  
  //add all messages to input queue
  if (n > 0) {
    char *line = strtok_r(bot->recvbuf, MSG_FOOTER, &bot->line_off);
    while (line) {
      BotInputQueue_enqueueInput(&bot->inputQueue, line);
      line = strtok_r(NULL, MSG_FOOTER, &bot->line_off);
    }
  }

  //grab next message in queue to process
  if (BotInputQueue_len(&bot->inputQueue) > 0) {
    BotQueuedInput *nextInput = BotInputQueue_dequeueInput(&bot->inputQueue);
    if (nextInput) {
      if ((n = bot_parse(bot, nextInput->msg)) < 0) return n;
      BotInput_freeQueuedInput(nextInput);
      nextInput = NULL;
    }
  }

  BotProcess_updateProcessQueue(&bot->procQueue, (void *)bot);
  processMsgQueueHash(bot);
  return 0;
}

void bot_join(BotInfo *bot, char *channel) {
  if (!channel) {
    fprintf(stderr, "bot_join: Cannot join NULL channel\n");
    return;
  }

  fprintf(stderr, "bot_join: %s...\n", channel);
  
  char sysBuf[MAX_MSG_LEN];
  snprintf(sysBuf, sizeof(sysBuf), JOIN_CMD_STR" %s", channel);
  bot_irc_send(bot, sysBuf);
  
  IrcMsg *msg = ircMsg_newMsg();
  ircMsg_setChannel(msg, channel);
  callback_call_r(bot->cb, CALLBACK_JOIN, (void*)bot, msg);
  free(msg);
}

/*
 * Keep a list of all nicks in the channel
 */
void bot_regName(BotInfo *bot, char *nick) {
  NickList *curNick;
  NickList *newNick = calloc(1, sizeof(NickList));
  if (!newNick) {
    perror("NickList Alloc Error: ");
    exit(1);
  }

  size_t diff = strcspn(nick, ILLEGAL_NICK_CHARS);
  nick += (diff == 0);
  fprintf(stderr, "Registering Nick: %s\n", nick);
  strncpy(newNick->nick, nick, MAX_NICK_LEN);
  if (!bot->names) {
    //first name
    bot->names = newNick;
    return;
  }

  curNick = bot->names;
  while (curNick->next) curNick = curNick->next;
  curNick->next = newNick;
}

void bot_rmName(BotInfo *bot, char *nick) {
  NickList *curNick, *lastNick;

  curNick = bot->names;
  lastNick = curNick;
  while (curNick && strncmp(curNick->nick, nick, MAX_NICK_LEN)) {
    fprintf(stderr, "Matching: %s with %s\n", curNick->nick, nick);
    lastNick = curNick;
    curNick = curNick->next;
  }

  //make sure the node we stopped on is the right one
  if (curNick && !strncmp(curNick->nick, nick, MAX_NICK_LEN)) {
    if (bot->names == curNick) bot->names = curNick->next;
    else lastNick->next = curNick->next;
    free(curNick);
  } else
    fprintf(stderr, "Failed to remove \'%s\' from nick list, does not exist\n", nick);

}

void bot_purgeNames(BotInfo *bot) {
  NickList *curNick = bot->names, *next;
  while (curNick) {
    next = curNick->next;
    free(curNick);
    curNick = next;
  }
  bot->names = NULL;
}


void bot_foreachName(BotInfo *bot, void *d, void (*fn) (NickList *nick, void *data)) {
  NickList *curNick = bot->names;
  while (curNick) {
    if (fn) fn(curNick, d);
    curNick = curNick->next;
  }
}

int bot_isThrottled(BotInfo *bot) {
  return bot->conInfo.isThrottled;
}

void bot_runProcess(BotInfo *bot, BotProcessFn fn, BotProcessArgs *args, char *cmd, char *caller) {
  unsigned int pid = BotProcess_queueProcess(&bot->procQueue, fn, args, cmd, caller);
  bot_send(bot, caller, ACTION_MSG, NULL, "%s: started '%s' with pid: %d.", caller, cmd, pid);
}


int bot_registerAlias(BotInfo *bot, char *alias, char *cmd) {
  return command_reg_alias(bot->commands, bot->cmdAliases, alias, cmd);
}
