#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>


#include "connection.h"

/*
 * Some nice wrappers for connecting to a specific address and port.
 */
int getConnectionInfo(const char *addr, const char *port, struct addrinfo **results) {
  struct addrinfo hints;
  
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
