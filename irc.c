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
 * Adds the required trailing '\r\n' to any message sent
 * to the irc server, and then procedes to send the message.
 */
int ircSend(int fd, const char *msg) {
  static char wrapped[MAX_MSG_LEN];
  size_t ilen = strlen(msg), wlen = strlen(MSG_FOOTER);
  
  if (ilen + wlen > MAX_MSG_LEN) {
    fprintf(stderr, MSG_LEN_EXCEED, MAX_MSG_LEN);
    return -1;
  }
  wlen += ilen;
  
  strncpy(wrapped, msg, wlen);
  strncat(wrapped, MSG_FOOTER, wlen);
  fprintf(stdout, "\nSENDING: %s", wrapped);
  return sendAll(fd, wrapped, wlen);
}

/*
 * Automatically formats a PRIVMSG command for the bot to speak.
 */
int _botSend(BotInfo *bot, char *target, char *fmt, va_list a) {
  char fmtBuf[MAX_MSG_LEN], buf[MAX_MSG_LEN];
  if (!target) target = bot->info->channel;
  snprintf(fmtBuf, sizeof(fmtBuf), "PRIVMSG %s :%s", target, fmt);
  vsnprintf(buf, sizeof(buf), fmtBuf, a); 
  return ircSend(bot->servfds.fd, buf);
}

int botSend(BotInfo *bot, char *target, char *fmt, ...) {
  int status = 0;
  va_list args;
  va_start(args, fmt);
  status = _botSend(bot, target, fmt, args);
  va_end(args);
  return status;
}

int ctcpSend(BotInfo *bot, char *target, char *command, char *msg, ...) {
  char outbuf[MAX_MSG_LEN];
  va_list args;
  va_start(args, msg);
  vsnprintf(outbuf, MAX_MSG_LEN, msg, args);
  va_end(args);
  return botSend(bot, target, CTCP_MARKER"%s %s"CTCP_MARKER, command, outbuf);
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
  
  callback_call(CALLBACK_SERVERCODE, (void *)bot, msg);
  free(msg);
  return 1;
}

int userJoined(BotInfo *bot, IrcMsg *msg) {
  bot_regName(bot, msg->nick);
  return callback_call(CALLBACK_USRJOIN, (void *)bot, msg);        
}

int userLeft(BotInfo *bot, IrcMsg *msg) {
  bot_rmName(bot, msg->nick);
  return callback_call(CALLBACK_USRPART, (void *)bot, msg);
}

int userNickChange(BotInfo *bot, IrcMsg *msg) {
  bot_rmName(bot, msg->nick);
  bot_regName(bot, msg->msg);
  return callback_call(CALLBACK_USRNICKCHANGE, (void *)bot, msg);
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
    ircSend(bot->servfds.fd, sysBuf);
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
    callback_call(CALLBACK_CONNECT, (void*)bot, NULL);
    bot->state = CONSTATE_CONNECTED;
    break;
    
  case CONSTATE_CONNECTED:
    //register the bot
    snprintf(sysBuf, sizeof(sysBuf), "NICK %s", bot->nick[bot->nickAttempt]);
    ircSend(bot->servfds.fd, sysBuf);
    snprintf(sysBuf, sizeof(sysBuf), "USER %s %s test: %s", bot->ident, bot->host, bot->realname);
    ircSend(bot->servfds.fd, sysBuf);
    //go to listening state to wait for registration confirmation
    bot->state = CONSTATE_LISTENING;
    break;

  case CONSTATE_REGISTERED:
    snprintf(sysBuf, sizeof(sysBuf), "JOIN %s", bot->info->channel);
    ircSend(bot->servfds.fd, sysBuf);
    bot->state = CONSTATE_JOINED;
    break;
  case CONSTATE_JOINED:
    callback_call(CALLBACK_JOIN, (void*)bot, NULL);
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
        callback_call(CALLBACK_MSG, (void*)bot, msg);

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
  
  bot->servfds.fd = clientInit(bot->info->server, bot->info->port, &bot->res);
  if (bot->servfds.fd < 0) exit(1);
  bot->state = CONSTATE_NONE;
  bot->servfds.events = POLLIN | POLLPRI | POLLOUT | POLLWRBAND;
  
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
}

void bot_cleanup(BotInfo *bot) {
  if (!bot) return;

  bot_purgeNames(bot);
  if (bot->commands) command_cleanup(bot->commands);
  bot->commands = NULL; 
  close(bot->servfds.fd);
  freeaddrinfo(bot->res);
}

void bot_addcommand(BotInfo *bot, char *cmd, int flags, int args, CommandFn fn) {
  command_reg(bot->commands, cmd, flags, args, fn);
}

/*
 * Run the bot! The bot will connect to the server and start
 * parsing replies.
 */
int bot_run(BotInfo *bot) {
  int n, ret;
  //process all input first before receiving more
  if (bot->line) {
    if ((ret = poll(&bot->servfds, 1, POLL_TIMEOUT_MS)) && bot->servfds.revents & POLLOUT) {
      if ((n = parse(bot, bot->line)) < 0) return n;
      bot->line = strtok_r(NULL, "\r\n", &bot->line_off);
    }
    return 0;
  }    
  
  bot->line_off = NULL;
  memset(bot->recvbuf, 0, sizeof(bot->recvbuf));
  if ((ret = poll(&bot->servfds, 1, POLL_TIMEOUT_MS)) && bot->servfds.revents & POLLIN) {
    n = recv(bot->servfds.fd, bot->recvbuf, sizeof(bot->recvbuf), 0);    
    if (!n) {
      printf("Remote closed connection\n");
      return -2;
    }
    else if (n < 0) {
      perror("Response error: ");
      return -3;
    }
  }
  
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



