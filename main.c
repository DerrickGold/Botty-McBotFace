#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#define MSG_LEN_EXCEED \
  "Message length exceeds %d byte limit. Message splitting not supported yet\n"

#define MAX_MSG_LEN 512
#define MAX_SERV_LEN 63
#define MAX_NICK_LEN 30
#define MAX_CHAN_LEN 50
#define MAX_CMD_LEN 9

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
  CALLBACK_COUNT,
} BotCallbackID;

//easy structure for reading details of an irc message
typedef struct IrcMsg {
  char nick[MAX_NICK_LEN];
  char command[MAX_CMD_LEN];
  char channel[MAX_CHAN_LEN];
  char msg[MAX_MSG_LEN];
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
 * Some nice wrappers for connecting to a specific address and port.
 */
int getConnectionInfo(const char *addr, const char *port, struct addrinfo **results) {
  struct addrinfo hints, *res;
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (addr == NULL)
    hints.ai_flags = AI_PASSIVE;

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
  size_t ilen = strlen(msg), wlen = ilen;
  char wrapped[MAX_MSG_LEN];
  
  if (ilen > MAX_MSG_LEN) {
    fprintf(stderr, MSG_LEN_EXCEED, MAX_MSG_LEN);
    return -1;
  }
  wlen += strlen(footer);
  
  strncpy(wrapped, msg, wlen);
  strncat(wrapped, footer, wlen);
  fprintf(stdout, "\nSENDING: %s", wrapped);
  n = sendAll(fd, wrapped, wlen);

  return n;
}

/*
 * Parses any incomming line from the irc server and 
 * invokes callbacks depending on the message type and
 * current state of the connection.
 */
int parse(IrcInfo *info, char *line) {
  if (!line) return 0;
  
  int n = 0;
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
    snprintf(sysBuf, sizeof(sysBuf), "PRIVMSG %s Hello, World!", info->channel);
    ircSend(info->servfd, sysBuf);
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
    } else
      callcb(CALLBACK_MSG, info, NULL);
 
    break;
  }
  return 0;
}

/*
 * Run the bot! The bot will connect to the server and start
 * parsing replies.
 */
void run(IrcInfo *info, int argc, char *argv[], int argstart) {
  struct addrinfo *res;
  char recvBuf[MAX_MSG_LEN];
  int n;
  char *line = NULL, *line_off = NULL;
  
  info->servfd = clientInit(info->server, info->port, &res);
  if (info->servfd < 0) exit(1);
  info->state = CONSTATE_NONE;
  
  while (1) {
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
      if (parse(info, line)) break;
      line = strtok_r(NULL, "\r\n", &line_off);
    }    
  }
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

int onMsg(IrcInfo *info, IrcMsg *msg) {
  printf("BOT HAS MESSAGE\n");
  //add some awesome logic here!
  return 0;
}


int main(int argc, char *argv[]) {

  IrcInfo conInfo = {
    .host = "CIRCBotHost",
    .nick = "CIrcBot",
    .port = "CHANGE THIS",
    .ident = "CIrcBot",
    .realname = "Botty McBotFace",
    .master = "CHANGE THIS",
    .server = "CHANGE THIS",
    .channel = "#CHANGETHIS",
    .cbfn = {0}
  };

  setcb(&conInfo, CALLBACK_CONNECT, &onConnect);
  setcb(&conInfo, CALLBACK_MSG, &onMsg);
  run(&conInfo, argc, argv, 0);
  
  return 0;
}

