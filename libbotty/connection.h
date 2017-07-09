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
  char enableSSL;
  int socket;
  SSL_CTX *ctx;
  SSL *ssl;
  struct addrinfo *res;
  struct pollfd servfds;
  int throttled, lastThrottled;
  char isThrottled;
} SSLConInfo;


int connection_ssl_client_init(const char *addr, const char *port, SSLConInfo *conInfo);

int connection_client_read(SSLConInfo *conInfo, char *buffer, size_t len);

int connection_client_poll(SSLConInfo *conInfo, int event, int *ret);

int connection_client_init(const char *addr, const char *port, struct addrinfo **res);

int connection_client_send(SSLConInfo *conInfo, char *data, size_t len);

#endif // __CONNECTION_H__
