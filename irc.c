#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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
  int n = 0;
  const char *footer = "\r\n";
  size_t ilen = strlen(msg), wlen = strlen(footer);
  char wrapped[MAX_MSG_LEN];
  
  if (ilen + wlen > MAX_MSG_LEN) {
    fprintf(stderr, MSG_LEN_EXCEED, MAX_MSG_LEN);
    return -1;
  }
  wlen += ilen;
  
  strncpy(wrapped, msg, wlen);
  strncat(wrapped, footer, wlen);
  fprintf(stdout, "\nSENDING: %s", wrapped);
  n = sendAll(fd, wrapped, wlen);

  return n;
}

/*
 * Automatically formats a PRIVMSG command for the bot to speak.
 */
int botSend(IrcInfo *info, char *target, char *msg) {
  char buf[MAX_MSG_LEN];
  if (!target) target = info->channel;
  snprintf(buf, sizeof(buf), "PRIVMSG %s :%s", target, msg);
  return ircSend(info->servfd, buf);
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
    return parse(info, line);
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
    ircSend(info->servfd, sysBuf);
    return 0;
  }
  
  if ((status = parseServer(info, line)) < 0) return -1;
  else if (status) return 0;
  
  switch (info->state) {
  case CONSTATE_NONE:
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
    ircSend(info->servfd, sysBuf);
    snprintf(sysBuf, sizeof(sysBuf), "USER %s %s test: %s", info->ident, info->host, info->realname);
    ircSend(info->servfd, sysBuf);
    //go to listening state to wait for registration confirmation
    info->state = CONSTATE_LISTENING;
    break;

  case CONSTATE_REGISTERED:
    snprintf(sysBuf, sizeof(sysBuf), "JOIN %s", info->channel);
    ircSend(info->servfd, sysBuf);
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
      IrcMsg *msg = newMsg(line, &cmd);
      
      if (!strcmp(msg->action, "JOIN"))
        status = callback_call(CALLBACK_USRJOIN, (void*)info, msg);        
      else if (!strcpy(msg->action, "PART"))
        status = callback_call(CALLBACK_USRPART, (void*)info, msg);
      else if (cmd) {
        CmdData data = { .info = info, .msg = msg };
        
        //make sure who ever is calling the command has permission to do so
        if (cmd->flags & CMDFLAG_MASTER && strcmp(msg->nick, info->master))
          fprintf(stderr, "%s is not %s\n", msg->nick, info->master);
        else if ((status = command_call(cmd->cmd, (void *)&data, msg->msgTok)) < 0)
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

/*
 * Run the bot! The bot will connect to the server and start
 * parsing replies.
 */
int run(IrcInfo *info, int argc, char *argv[], int argstart) {
  struct addrinfo *res;
  char stayAlive = 1;
  char recvBuf[MAX_MSG_LEN];
  int n;
  char *line = NULL, *line_off = NULL;

  info->servfd = clientInit(info->server, info->port, &res);
  if (info->servfd < 0) exit(1);
  info->state = CONSTATE_NONE;

  n = strlen(SERVER_PREFIX);
  if (strncmp(SERVER_PREFIX, info->server, n)) {
    int servLen = strlen(info->server);
    if (servLen + n < MAX_SERV_LEN) {
      memmove(info->server + n, info->server, servLen);
      memcpy(info->server, SERVER_PREFIX, n);
      printf("NEW SERVER NAME: %s\n", info->server);
    }
  }  

  while (stayAlive) {
    line_off = NULL;
    memset(recvBuf, 0, sizeof(recvBuf));
    n = recv(info->servfd, recvBuf, sizeof(recvBuf), 0);
    if (!n) {
      printf("Remote closed connection\n");
      break;
    }
    else if (n < 0) {
      perror("Response error: ");
      break;
    }

    //parse replies one line at a time
    line = strtok_r(recvBuf, "\r\n", &line_off);
    while (line) {
      if (parse(info, line) < 0) {
        stayAlive = 0;
        break;
      }
      line = strtok_r(NULL, "\r\n", &line_off);
    }    
  }
  close(info->servfd);
  freeaddrinfo(res);
  return 0;
}
