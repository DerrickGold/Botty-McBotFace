#ifndef __CONNECTION_H__
#define  __CONNECTION_H__

#include "globals.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>


typedef struct SSLConInfo {
  int socket;
  SSL_CTX *ctx;
  SSL *ssl;
  struct addrinfo *res;
  struct pollfd servfds;
} SSLConInfo;


extern int clientInit_ssl(const char *addr, const char *port, SSLConInfo *conInfo);

extern int clientRead(SSLConInfo *conInfo, char *buffer, size_t len);

extern int clientWrite(SSLConInfo *conInfo, char *buffer, size_t len);

extern int clientPoll(SSLConInfo *conInfo, int event, int *ret);

extern int getConnectionInfo(const char *addr, const char *port, struct addrinfo **results);

extern int socketConnect(int sockfd, struct addrinfo *res);

extern int initSockCon(struct addrinfo *res, int (*action)(int, struct addrinfo *));

extern int clientInit(const char *addr, const char *port, struct addrinfo **res);

extern int sendAll(SSLConInfo *conInfo, char *data, size_t len);

#endif // __CONNECTION_H__
