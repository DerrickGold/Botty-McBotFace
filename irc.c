#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

#include "irc.h"
#include "ircmsg.h"
#include "commands.h"
#include "callback.h"
#include "connection.h"
#include "cmddata.h"

int parse(IrcInfo *info, char *line);

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
int _botSend(IrcInfo *info, char *target, char *fmt, va_list a) {
  char fmtBuf[MAX_MSG_LEN], buf[MAX_MSG_LEN];
  if (!target) target = info->channel;
  snprintf(fmtBuf, sizeof(fmtBuf), "PRIVMSG %s :%s", target, fmt);
  vsnprintf(buf, sizeof(buf), fmtBuf, a); 
  return ircSend(info->servfds.fd, buf);
}

int botSend(IrcInfo *info, char *target, char *fmt, ...) {
  int status = 0;
  va_list args;
  va_start(args, fmt);
  status = _botSend(info, target, fmt, args);
  va_end(args);
  return status;
}

int ctcpSend(IrcInfo *info, char *target, char *command, char *msg, ...) {
  char outbuf[MAX_MSG_LEN];
  va_list args;
  va_start(args, msg);
  vsnprintf(outbuf, MAX_MSG_LEN, msg, args);
  va_end(args);
  return botSend(info, target, CTCP_MARKER"%s %s"CTCP_MARKER, command, outbuf);
}

/*
 * Default actions for handling various server responses such as nick collisions
 */
static int defaultServActions(IrcInfo *info, IrcMsg *msg, char *line) {
  //if nick is already registered, try a new one
  if (!strncmp(msg->action, REG_ERR_CODE, strlen(REG_ERR_CODE))) {
    if (info->nickAttempt < NICK_ATTEMPTS) info->nickAttempt++;
    else {
      fprintf(stderr, "Exhuasted nick attempts, please configure a unique nick\n");
      return -1;
    }
    fprintf(stderr, "Nick is already in use, attempting to use: %s\n", info->nick[info->nickAttempt]);
    //then attempt registration again
    info->state = CONSTATE_CONNECTED;
    //return parse(info, line);
    return 0;
  }
  //otherwise, nick is not in use
  else if (!strncmp(msg->action, REG_SUC_CODE, strlen(REG_SUC_CODE))) {
    info->state = CONSTATE_REGISTERED;
  }
  
  return 0;
}


/*
 * Parse out any server responses that may need to be attended to
 * and pass them into the appropriate callbacks.
 */
static int parseServer(IrcInfo *info, char *line) {
  char buf[MAX_MSG_LEN];
  snprintf(buf, sizeof(buf), ":%s", info->server);
  //not a server response
  if (strncmp(line, buf, strlen(buf))) return 0;
  //is a server response
  strncpy(buf, line, MAX_MSG_LEN);
  IrcMsg *msg = servMsg(buf);
  int status = defaultServActions(info, msg, line);
  if (status) {
    free(msg);
    return status;
  }
  
  callback_call(CALLBACK_SERVERCODE, (void *)info, msg);
  free(msg);
  return 0;
}

/*
 * Parses any incomming line from the irc server and 
 * invokes callbacks depending on the message type and
 * current state of the connection.
 */
int parse(IrcInfo *info, char *line) {
  if (!line) return 0;
  
  int status = 0;
  char sysBuf[MAX_MSG_LEN];
  char *space = NULL, *space_off = NULL;
  fprintf(stdout, "SERVER: %s\n", line);
  
  //respond to server pings
  if (!strncmp(line, "PING", strlen("PING"))) {
    char *pong = line + strlen("PING") + 1;
    snprintf(sysBuf, sizeof(sysBuf), "PONG %s", pong);
    ircSend(info->servfds.fd, sysBuf);
    return 0;
  }
  
  if ((status = parseServer(info, line)) < 0) return -1;
  else if (status) return 0;
  
  switch (info->state) {
  case CONSTATE_NONE:
    if (!info->commands) info->commands = command_global();
    //initialize data here
    space = strtok_r(line, " ", &space_off);
    if (space) {
      //grab new server name if we've been redirected
      memcpy(info->server, space+1, strlen(space) - 1);
      printf("given server: %s\n", info->server);
    }
    callback_call(CALLBACK_CONNECT, (void*)info, NULL);
    info->state = CONSTATE_CONNECTED;
    break;
    
  case CONSTATE_CONNECTED:
    //register the bot
    snprintf(sysBuf, sizeof(sysBuf), "NICK %s", info->nick[info->nickAttempt]);
    ircSend(info->servfds.fd, sysBuf);
    snprintf(sysBuf, sizeof(sysBuf), "USER %s %s test: %s", info->ident, info->host, info->realname);
    ircSend(info->servfds.fd, sysBuf);
    //go to listening state to wait for registration confirmation
    info->state = CONSTATE_LISTENING;
    break;

  case CONSTATE_REGISTERED:
    snprintf(sysBuf, sizeof(sysBuf), "JOIN %s", info->channel);
    ircSend(info->servfds.fd, sysBuf);
    info->state = CONSTATE_JOINED;
    break;
  case CONSTATE_JOINED:
    callback_call(CALLBACK_JOIN, (void*)info, NULL);
    info->state = CONSTATE_LISTENING;
    break;
  default:
  case CONSTATE_LISTENING:
    snprintf(sysBuf, sizeof(sysBuf), ":%s", info->nick[info->nickAttempt]);
    if (!strncmp(line, sysBuf, strlen(sysBuf))) {
      //filter out messages that the bot says itself
      break;
    }
    else {
      BotCmd *cmd = NULL;
      IrcMsg *msg = newMsg(line, info->commands, &cmd);
      
      if (!strcmp(msg->action, "JOIN"))
        status = callback_call(CALLBACK_USRJOIN, (void*)info, msg);        
      else if (!strcpy(msg->action, "PART"))
        status = callback_call(CALLBACK_USRPART, (void*)info, msg);
      else if (cmd) {
        CmdData data = { .info = info, .msg = msg };
        
        //make sure who ever is calling the command has permission to do so
        if (cmd->flags & CMDFLAG_MASTER && strcmp(msg->nick, info->master))
          fprintf(stderr, "%s is not %s\n", msg->nick, info->master);
        else if ((status = command_call_r(info->commands, cmd->cmd, (void *)&data, msg->msgTok)) < 0)
          fprintf(stderr, "Command '%s' gave exit code\n,", cmd->cmd);
      }
      else 
        callback_call(CALLBACK_MSG, (void*)info, msg);

      free(msg);
    } 
    break;
  }
  return status;
}

int bot_connect(IrcInfo *info, int argc, char *argv[], int argstart) {
  if (!info) return -1;
  
  info->servfds.fd = clientInit(info->server, info->port, &info->res);
  if (info->servfds.fd < 0) exit(1);
  info->state = CONSTATE_NONE;
  info->servfds.events = POLLIN | POLLPRI | POLLOUT | POLLWRBAND;
  
  int n = strlen(SERVER_PREFIX);
  if (strncmp(SERVER_PREFIX, info->server, n)) {
    int servLen = strlen(info->server);
    if (servLen + n < MAX_SERV_LEN) {
      memmove(info->server + n, info->server, servLen);
      memcpy(info->server, SERVER_PREFIX, n);
      printf("NEW SERVER NAME: %s\n", info->server);
    }
  }  

  return 0;
}

void bot_cleanup(IrcInfo *info) {
  if (!info) return;
  close(info->servfds.fd);
  freeaddrinfo(info->res);
}

void bot_addcommand(IrcInfo *info, char *cmd, int flags, int args, CommandFn fn) {
  command_reg_r(&info->commands, cmd, flags, args, fn);
}

/*
 * Run the bot! The bot will connect to the server and start
 * parsing replies.
 */
int bot_run(IrcInfo *info) {
  int n, ret;
  //process all input first before receiving more
  if (info->line) {
    if ((ret = poll(&info->servfds, 1, POLL_TIMEOUT_MS)) && info->servfds.revents & POLLOUT) {
      if ((n = parse(info, info->line)) < 0) return n;
      info->line = strtok_r(NULL, "\r\n", &info->line_off);
    }
    return 0;
  }    
  
  info->line_off = NULL;
  memset(info->recvbuf, 0, sizeof(info->recvbuf));
  if ((ret = poll(&info->servfds, 1, POLL_TIMEOUT_MS)) && info->servfds.revents & POLLIN) {
    n = recv(info->servfds.fd, info->recvbuf, sizeof(info->recvbuf), 0);    
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
  info->line = strtok_r(info->recvbuf, "\r\n", &info->line_off);
  return 0;
}
