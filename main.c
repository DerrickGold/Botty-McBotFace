#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#define MSG_LEN_EXCEED \
  "Message length exceeds %d byte limit. Message splitting not supported yet\n"

#define CMD_CHAR '~'
#define MAX_MSG_LEN 512
#define MAX_SERV_LEN 63
#define MAX_NICK_LEN 30
#define MAX_CHAN_LEN 50
#define MAX_CMD_LEN 9
#define BOT_ARG_DELIM ' '
#define MAX_BOT_ARGS 8

typedef enum {
  CONSTATE_NONE,
  CONSTATE_CONNECTED,
  CONSTATE_REGISTERED,
  CONSTATE_JOINED,
  CONSTATE_LISTENING,
} ConState;

//Used in conjunction with cbfn in IrcInfo
typedef enum {
  CALLBACK_CONNECT,
  CALLBACK_JOIN,
  CALLBACK_MSG,
  CALLBACK_USRJOIN,
  CALLBACK_USRPART,
  CALLBACK_COUNT,
} BotCallbackID;

typedef enum {
  CMDFLAG_MASTER = (1<<0),
} CommandFlags;


typedef struct BotCmd {
  int flags;
  char cmd[MAX_CMD_LEN];
  int args;
  int (*fn)(void *, char *a[MAX_BOT_ARGS]);
  struct BotCmd *next;
} BotCmd;

//easy structure for reading details of an irc message
typedef struct IrcMsg {
  char nick[MAX_NICK_LEN];
  char action[MAX_CMD_LEN];
  char channel[MAX_CHAN_LEN];
  char msg[MAX_MSG_LEN];
  char *msgTok[MAX_BOT_ARGS];
} IrcMsg;

typedef struct IrcInfo {
  char host[256];
  char nick[MAX_NICK_LEN];
  char port[6];
  char ident[10];
  char realname[64];
  char master[30];
  char server[MAX_SERV_LEN];
  char channel[MAX_CHAN_LEN];
  int servfd;
  ConState state;
  int (*cbfn[CALLBACK_COUNT])(struct IrcInfo *, IrcMsg *);
} IrcInfo;

static BotCmd *GlobalCmds = NULL;

/*
 * Setter and callers for callbacks assigned to a given IrcInfo instance.
 */
void setcb(IrcInfo *info, BotCallbackID id, int (*fn)(IrcInfo *, IrcMsg *)) {
  if (id > CALLBACK_COUNT) {
    fprintf(stderr, "setcb: Callback ID %d does not exist\n", (int)id);
    return;
  }
  info->cbfn[id] = fn;
}

int callcb(BotCallbackID id, IrcInfo *info, IrcMsg *msg) {
  if (id > CALLBACK_COUNT) {
    fprintf(stderr, "callcb: Callback ID %d does not exist\n", (int)id);
    return -1;
  }

  if (info->cbfn[id]) return info->cbfn[id](info, msg);
  return -1;
}

/*
 * Register a command for the bot to use
 */
void regcmd(BotCmd **commands, char *cmdtag, int flags, int args, int (*fn)(void *, char *a[MAX_BOT_ARGS])) {
  if (!commands) return;

  BotCmd *curCmd;
  BotCmd *newcmd = calloc(1, sizeof(BotCmd));
  if (!newcmd) {
    perror("Command Alloc Error: ");
    exit(1);
  }
  
  strncpy(newcmd->cmd, cmdtag, MAX_CMD_LEN);
  newcmd->args = args;
  newcmd->fn = fn;
  newcmd->flags = flags;
  
  if (!*commands) {
    //first command
    *commands = newcmd;
    return;
  }
  
  curCmd = *commands;
  while (curCmd->next) curCmd = curCmd->next;
  curCmd->next = newcmd;
}

BotCmd *getcmd(BotCmd *commands, char *command) {
  BotCmd *curcmd = commands;
  while (curcmd && strncmp(curcmd->cmd, command, MAX_CMD_LEN))
    curcmd = curcmd->next;

  return curcmd;
}

int callcmd(BotCmd *commands, char *command, IrcInfo *info, char *args[MAX_BOT_ARGS]) {

  BotCmd *curcmd = getcmd(commands, command);
  if (!curcmd) {
    fprintf(stderr, "Command (%s) is not a registered command\n", command);
    return -1;
  }
  
  return curcmd->fn((void *)info, args);
}



IrcMsg *newMsg(char *input, BotCmd **cmd) {
  IrcMsg *msg = NULL;
  char *end = input + strlen(input);
  char *tok = NULL, *tok_off = NULL;
  int i = 0;
  
  msg = calloc(1, sizeof(IrcMsg));
  if (!msg) {
    fprintf(stderr, "msg alloc error\n");
    exit(1);
  }

  //first get the nick that created the message
  tok = strtok_r(input, "!", &tok_off);
  if (!tok) return msg;
  strncpy(msg->nick, tok+1, MAX_NICK_LEN);
  //skip host name
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok) return msg;

  //get action issued
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok) return msg;
  strncpy(msg->action, tok, MAX_CMD_LEN);

  //get the channel or user the message originated from
  tok = strtok_r(NULL, " ", &tok_off);
  if (!tok) return msg;
  strncpy(msg->channel, tok, MAX_CHAN_LEN);

  if (!tok_off || tok_off + 1 >= end) return msg;
  
  //finally save the rest of the message
  strncpy(msg->msg, tok_off+1, MAX_MSG_LEN);

  //parse a given command
  if (msg->msg[0] == CMD_CHAR && cmd) {
    int argCount = MAX_BOT_ARGS;
    tok = msg->msg + 1;
    while(i < argCount) {
      tok_off = strchr(tok, BOT_ARG_DELIM);
      if (tok_off && i < argCount - 1) *tok_off = '\0';
      msg->msgTok[i] = tok;
    
      if (i == 0) {
        *cmd = getcmd(GlobalCmds, msg->msgTok[0]);
        if (*cmd)  argCount = (*cmd)->args;
      }
      
      if (!tok_off) break;
      tok_off++;
      tok = tok_off;
      i++;
    }
  }

  return msg;
}


/*
 * Some nice wrappers for connecting to a specific address and port.
 */
int getConnectionInfo(const char *addr, const char *port, struct addrinfo **results) {
  struct addrinfo hints, *res;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (addr == NULL)
    return -1;

  int status = 0;
  if ((status = getaddrinfo(addr, port, &hints, results)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    return -1;
  }

  return 0;
}

int socketConnect(int sockfd, struct addrinfo *res) {
  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    close(sockfd);
    fprintf(stderr, "connect: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}


int initSockCon(struct addrinfo *res, int (*action)(int, struct addrinfo *)) {
  struct addrinfo *p = NULL;
  int sockfd = -1;

  if (!action) {
    fprintf(stderr, "No socket initialization action provided.\n");
    return -1;
  }
  
  for (p = res; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("socket error: ");
      continue;
    }
    if (action(sockfd, res) == -1) continue;
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "Failed to apply socket action\n");
    return -1;
  }

  return sockfd;
}


/*
 * Initialize the client networking by making a connection to the specified server.
 */
int clientInit(const char *addr, const char *port, struct addrinfo **res) {
  if (getConnectionInfo(addr, port, res)) return -1;

  int sockfd = -1;
  if ((sockfd = initSockCon(*res, &socketConnect)) == -1) {
    fprintf(stderr, "Failed to connect to socket\n");
    return -1;
  }

  return sockfd;
}


int sendAll(int sockfd, char *data, size_t len) {
  size_t total = 0, bytesLeft = len;
  int n = 0;
  while (total < len) {
    n = send(sockfd, data+total, bytesLeft, 0);
    if (n == -1) break;
    total += n;
    bytesLeft -= n;
  }
  return n==-1?-1:0;
}

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

int botSend(IrcInfo *info, char *target, char *msg) {
  char buf[MAX_MSG_LEN];
  if (!target) target = info->channel;
  snprintf(buf, sizeof(buf), "PRIVMSG %s %s", target, msg);
  return ircSend(info->servfd, buf);
}

/*
 * Parses any incomming line from the irc server and 
 * invokes callbacks depending on the message type and
 * current state of the connection.
 */
int parse(IrcInfo *info, char *line) {
  if (!line) return 0;
  
  int n = 0, status = 0;
  char sysBuf[MAX_MSG_LEN];
  fprintf(stdout, "SERVER: %s\n", line);
  
  //respond to server pings
  if (!strncmp(line, "PING", strlen("PING"))) {
    char *pong = line + strlen("PING") + 1;
    snprintf(sysBuf, sizeof(sysBuf), "PONG %s", pong);
    ircSend(info->servfd, sysBuf);
    return 0;
  }
  
  switch (info->state) {
  case CONSTATE_NONE:
    //no response errors received at this point, hostname should be
    //determined at this point
    info->state = CONSTATE_CONNECTED;
    break;
    
  case CONSTATE_CONNECTED:
    callcb(CALLBACK_CONNECT, info, NULL);
    //register the bot
    snprintf(sysBuf, sizeof(sysBuf), "NICK %s", info->nick);
    ircSend(info->servfd, sysBuf);
    snprintf(sysBuf, sizeof(sysBuf), "USER %s %s test: %s", info->ident, info->host, info->realname);
    ircSend(info->servfd, sysBuf);
    info->state = CONSTATE_REGISTERED;
    break;
    
  case CONSTATE_REGISTERED:
    snprintf(sysBuf, sizeof(sysBuf), "JOIN %s", info->channel);
    ircSend(info->servfd, sysBuf);
    info->state = CONSTATE_JOINED;
    break;
  case CONSTATE_JOINED:
    callcb(CALLBACK_JOIN, info, NULL);
    info->state = CONSTATE_LISTENING;
    break;
  default:
  case CONSTATE_LISTENING:
    snprintf(sysBuf, sizeof(sysBuf), ":irc.%s", info->server);
    if (!strncmp(line, sysBuf, strlen(sysBuf))) {
      //filter out messages from the server
      break;
    }
    
    snprintf(sysBuf, sizeof(sysBuf), ":%s", info->nick);
    if (!strncmp(line, sysBuf, strlen(sysBuf))) {
      //filter out messages that the bot says itself
      break;
    }
    else {
      BotCmd *cmd = NULL;
      IrcMsg *msg = newMsg(line, &cmd);
      
      if (!strcmp(msg->action, "JOIN"))
        status = callcb(CALLBACK_USRJOIN, info, msg);
        
      else if (!strcpy(msg->action, "PART"))
        status = callcb(CALLBACK_USRPART, info, msg);
      
      else {
        if (cmd) {
          //make sure who ever is calling the command has permission to do so
          if (cmd->flags & CMDFLAG_MASTER && strcmp(msg->nick, info->master))
            fprintf(stderr, "%s is not %s\n", msg->nick, info->master);
          else if ((status = callcmd(GlobalCmds, cmd->cmd, info, msg->msgTok)) < 0)
            fprintf(stderr, "Command '%s' gave exit code\n,", cmd->cmd);
        }
        else 
          callcb(CALLBACK_MSG, info, msg);
      }
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
void run(IrcInfo *info, int argc, char *argv[], int argstart) {
  struct addrinfo *res;
  char stayAlive = 1;
  char recvBuf[MAX_MSG_LEN];
  int n;
  char *line = NULL, *line_off = NULL;
  
  info->servfd = clientInit(info->server, info->port, &res);
  if (info->servfd < 0) exit(1);
  info->state = CONSTATE_NONE;
  
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
}


/*
 * Callback functions can be used for adding
 * features or logic to notable  responses or events.
 */
int onConnect(IrcInfo *info, IrcMsg *msg) {
  printf("BOT HAS CONNECTED!\n");
  return 0;
}

int onJoin(IrcInfo *info, IrcMsg *msg) {
  botSend(info, NULL, "Hello, World!");
  return 0;
}

int onMsg(IrcInfo *info, IrcMsg *msg) {
  if (!info || !msg) return -1;
  printf("Recieved msg from %s in %s\n", msg->nick, msg->channel);
  printf("%s\n", msg->msg);
  return 0;
}

int onUsrJoin(IrcInfo *info, IrcMsg *msg) {
  if (!info || !msg) return -1;
  printf("%s has joined the channel\n", msg->nick);
  return 0;
}

int onUsrPart(IrcInfo *info, IrcMsg *msg) {
  if (!info || !msg) return -1;

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
  botSend((IrcInfo *)i, NULL, "Seeya!");
  return -1;
}


int main(int argc, char *argv[]) {

  IrcInfo conInfo = {
    .host = "CIRCBotHost",
    .nick = "CIrcBot",
    .port = "6667",
    .ident = "CIrcBot",
    .realname = "Botty McBotFace",
    .master = "Derrick",
    .server = "CHANGE THIS",
    .channel = "#CHANGETHIS",
    .cbfn = {0}
  };

  setcb(&conInfo, CALLBACK_CONNECT, &onConnect);
  setcb(&conInfo, CALLBACK_JOIN, &onJoin);
  setcb(&conInfo, CALLBACK_MSG, &onMsg);
  setcb(&conInfo, CALLBACK_USRJOIN, &onUsrJoin);
  setcb(&conInfo, CALLBACK_USRPART, &onUsrPart);
  
  //register some commands
  regcmd(&GlobalCmds, "say", 0, 2, &botcmd_say);
  regcmd(&GlobalCmds, "die", CMDFLAG_MASTER, 1, &botcmd_die);
  
  run(&conInfo, argc, argv, 0);
  return 0;
}

