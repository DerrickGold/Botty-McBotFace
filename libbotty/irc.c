#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

#include "builtin.h"
#include "irc.h"
#include "ircmsg.h"
#include "commands.h"
#include "callback.h"
#include "connection.h"
#include "cmddata.h"

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

int parse(BotInfo *bot, char *line);



/*
 * Send an irc formatted message to the server.
 * Assumes your message is appropriately sized for a single
 * message.
 */
static int _send(SSLConInfo *conInfo, char *command, char *target, char *msg, char *ctcp) {
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

  fprintf(stdout, "SENDING (%d bytes): %s\n", written, curSendBuf);
  //return sendAll(fd, curSendBuf, written);
  return sendAll(conInfo, curSendBuf, written);
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
int ircSend_s(SSLConInfo *conInfo, char *command, char *target, char *msg, char *ctcp, char *nick) {
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
      _send(conInfo, command, target, nextMsg, ctcp);
      nextMsg = end;
      if (end < last) *end = replaced;
      //remove any leading spaces for the next message
      if (*nextMsg == ' ') nextMsg++;
    } while (--chunks && nextMsg < last);
    
    return 0;
  }
    
  return _send(conInfo, command, target, msg, ctcp);
}

int ircSend(SSLConInfo *conInfo, char *msg) {
  return _send(conInfo, NULL, NULL, msg, NULL);
}


/*
 * Automatically formats a PRIVMSG command for the bot to speak.
 */

int _botSend(BotInfo *bot, char *target, char *action, char *ctcp, char *fmt, va_list a) {
  char *msgBuf;
  if (!target) target = bot->info->channel;

  //only buffer up to 4 message splits worth of text
  size_t msgBufLen = MAX_MSG_LEN * MAX_MSG_SPLITS;
  msgBuf = malloc(msgBufLen);
  if (!msgBuf) {
    perror("Msg alloc failed:");
    return -1;
  }
  vsnprintf(msgBuf, msgBufLen - 1, fmt, a);
  int status = ircSend_s(&bot->conInfo, action, target, msgBuf, ctcp, bot_getNick(bot));
  free(msgBuf);
  return status;
}

int botSend(BotInfo *bot, char *target, char *action, char *ctcp, char *fmt, ...) {
  int status = 0;
  va_list args;
  va_start(args, fmt);
  status = _botSend(bot, target, action, ctcp, fmt, args);
  va_end(args);
  return status;
}

int ctcpSend(BotInfo *bot, char *target, char *command, char *msg, ...) {
  char outbuf[MAX_MSG_LEN];
  va_list args;
  va_start(args, msg);
  vsnprintf(outbuf, MAX_MSG_LEN, msg, args);
  va_end(args);
  return botSend(bot, target, ACTION_MSG, NULL, CTCP_MARKER"%s %s"CTCP_MARKER, command, outbuf);
}

/*
 * Default actions for handling various server responses such as nick collisions
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
    //return parse(bot, line);
    return 0;
  }
  //otherwise, nick is not in use
  else if (!strncmp(msg->action, REG_SUC_CODE, strlen(REG_SUC_CODE))) {
    bot->state = CONSTATE_REGISTERED;
  }
  //store all current users in the channel
  else if (!strncmp(msg->action, NAME_REPLY, strlen(NAME_REPLY))) {
    char *start = msg->msgTok[1], *next = start, *end = start + strlen(start) - 1;
    while (start < end) {
      while (*next != BOT_ARG_DELIM && next < end) next++;
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
  IrcMsg *msg = servMsg(buf);
  int status = defaultServActions(bot, msg, line);
  if (status) {
    free(msg);
    return status;
  }
  
  callback_call_r(bot->cb, CALLBACK_SERVERCODE, (void *)bot, msg);
  free(msg);
  return 1;
}

int userJoined(BotInfo *bot, IrcMsg *msg) {
  bot_regName(bot, msg->nick);
  return callback_call_r(bot->cb, CALLBACK_USRJOIN, (void *)bot, msg);        
}

int userLeft(BotInfo *bot, IrcMsg *msg) {
  bot_rmName(bot, msg->nick);
  return callback_call_r(bot->cb, CALLBACK_USRPART, (void *)bot, msg);
}

int userNickChange(BotInfo *bot, IrcMsg *msg) {
  bot_rmName(bot, msg->nick);
  bot_regName(bot, msg->msg);
  return callback_call_r(bot->cb, CALLBACK_USRNICKCHANGE, (void *)bot, msg);
}

/*
 * Parses any incomming line from the irc server and 
 * invokes callbacks depending on the message type and
 * current state of the connection.
 */
int parse(BotInfo *bot, char *line) {
  if (!line) return 0;
  
  int servStat = 0;
  char sysBuf[MAX_MSG_LEN];
  char *space = NULL, *space_off = NULL;
  fprintf(stdout, "SERVER: %s\n", line);
  
  //respond to server pings
  if (!strncmp(line, "PING", strlen("PING"))) {
    char *pong = line + strlen("PING") + 1;
    snprintf(sysBuf, sizeof(sysBuf), "PONG %s", pong);
    ircSend(&bot->conInfo, sysBuf);
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
      printf("given server: %s\n", bot->info->server);
    }
    callback_call_r(bot->cb, CALLBACK_CONNECT, (void*)bot, NULL);
    bot->state = CONSTATE_CONNECTED;
    break;
    
  case CONSTATE_CONNECTED:
    //register the bot
    snprintf(sysBuf, sizeof(sysBuf), "NICK %s", bot->nick[bot->nickAttempt]);
    ircSend(&bot->conInfo, sysBuf);
    snprintf(sysBuf, sizeof(sysBuf), "USER %s %s test: %s", bot->ident, bot->host, bot->realname);
    ircSend(&bot->conInfo, sysBuf);
    //go to listening state to wait for registration confirmation
    bot->state = CONSTATE_LISTENING;
    break;

  case CONSTATE_REGISTERED:
    snprintf(sysBuf, sizeof(sysBuf), "JOIN %s", bot->info->channel);
    ircSend(&bot->conInfo, sysBuf);
    bot->state = CONSTATE_JOINED;
    break;
  case CONSTATE_JOINED:
    callback_call_r(bot->cb, CALLBACK_JOIN, (void*)bot, NULL);
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
      //ignore non-critical events while processing
      if (bot_isProcessing(bot)) break;
      
      BotCmd *cmd = NULL;
      IrcMsg *msg = newMsg(line, bot->commands, &cmd);
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
int irc_init(void) {
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

void irc_cleanup(void) {
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
  return 0;
}

int bot_connect(BotInfo *bot) {
  if (!bot) return -1;

#if defined(USE_OPENSSL)
  if (clientInit_ssl(bot->info->server, bot->info->port, &bot->conInfo)) exit(1);
  bot->state = CONSTATE_NONE;
  return 0;
#else
  
  bot->conInfo.servfds.fd = clientInit(bot->info->server, bot->info->port, &bot->conInfo.res);
  if (bot->conInfo.servfds.fd < 0) exit(1);
  bot->state = CONSTATE_NONE;
  bot->conInfo.servfds.events = POLLIN | POLLPRI | POLLOUT | POLLWRBAND;
  
  int n = strlen(SERVER_PREFIX);
  if (strncmp(SERVER_PREFIX, bot->info->server, n)) {
    int servLen = strlen(bot->info->server);
    if (servLen + n < MAX_SERV_LEN) {
      memmove(bot->info->server + n, bot->info->server, servLen);
      memcpy(bot->info->server, SERVER_PREFIX, n);
      printf("NEW SERVER NAME: %s\n", bot->info->server);
    }
  }  
  return 0;
#endif
}

char *bot_getNick(BotInfo *bot) {
  return bot->nick[bot->nickAttempt];
}

void bot_cleanup(BotInfo *bot) {
  if (!bot) return;

  bot_purgeNames(bot);
  if (bot->commands) command_cleanup(bot->commands);
  bot->commands = NULL; 
  close(bot->conInfo.servfds.fd);
  freeaddrinfo(bot->conInfo.res);
}

void bot_setCallback(BotInfo *bot, BotCallbackID id, Callback fn) {
  callback_set_r(bot->cb, id, fn);
}

void bot_addcommand(BotInfo *bot, char *cmd, int flags, int args, CommandFn fn) {
  command_reg(bot->commands, cmd, flags, args, fn);
}

void bot_setProcess(BotInfo *bot, BotProcessFn fn, void *args) {
  bot->process.fn = fn;
  bot->process.arg = args;
  bot->process.busy = 1;
}

void bot_clearProcess(BotInfo *bot) {
  bot->process.fn = NULL;
  bot->process.arg = NULL;
  bot->process.busy = 0;
}

void bot_runProcess(BotInfo *bot) {
  if (bot->process.fn) {
    if ((bot->process.busy = bot->process.fn((void *)bot, bot->process.arg)) < 0)
      bot_clearProcess(bot);
  }
}

int bot_isProcessing(BotInfo *bot) {
  return bot->process.busy > 0;
}

/*
 * Run the bot! The bot will connect to the server and start
 * parsing replies.
 */
int bot_run(BotInfo *bot) {
  int n = 0, ret = 0;

  bot_runProcess(bot);
  //process all input first before receiving more
  if (bot->line) {
    if (clientPoll(&bot->conInfo, POLLOUT, &ret)) {
      if ((n = parse(bot, bot->line)) < 0) return n;
      bot->line = strtok_r(NULL, "\r\n", &bot->line_off);
    }
    return 0;
  }    
  
  bot->line_off = NULL;
  memset(bot->recvbuf, 0, sizeof(bot->recvbuf));
  
  if (clientPoll(&bot->conInfo, POLLIN, &ret)) {
    n = clientRead(&bot->conInfo, bot->recvbuf, sizeof(bot->recvbuf));
    if (!n) {
      printf("Remote closed connection\n");
      return -2;
    }
    else if (n < 0) {
      perror("Response error: ");
      return -3;
    }
  }
  fprintf(stderr, "Polling input: %d\n", ret);
  
  //parse replies one line at a time
  bot->line = strtok_r(bot->recvbuf, "\r\n", &bot->line_off);
  return 0;
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

  while (curNick && strncmp(curNick->nick, nick, MAX_NICK_LEN)) {
    lastNick = curNick;
    curNick = curNick->next;
  }

  //make sure the node we stopped on is the right one
  if (!strncmp(curNick->nick, nick, MAX_NICK_LEN)) {
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
    free(next);
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
