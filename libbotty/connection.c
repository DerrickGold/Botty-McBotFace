#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "connection.h"

static int setNonBlock(int fd, char value) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return errno;

    if (value)
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
}

/*
 * Some nice wrappers for connecting to a specific address and port.
 */
static int getConnectionInfo(const char *addr, const char *port, struct addrinfo **results) {
  struct addrinfo hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
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

static int socketConnect(int sockfd, struct addrinfo *res) {
  setNonBlock(sockfd, 1);

  int r = connect(sockfd, res->ai_addr, res->ai_addrlen);
  if (r < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
    struct pollfd pfd;
    pfd.fd = sockfd;
    pfd.events = POLLOUT | POLLERR;
    while (r == 0) {
      r = poll(&pfd, 1, 100);
    }

    if (r != POLLOUT) {
        close(sockfd);
        fprintf(stderr, "connect: %s: %d\n", strerror(errno), errno);
    }
  }


  fprintf(stderr, "done: %s: %d\n", strerror(errno), errno);
  return 0;
}


static int initSockCon(struct addrinfo *res, int (*action)(int, struct addrinfo *)) {
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
int connection_client_init(const char *addr, const char *port, struct addrinfo **res) {
  if (getConnectionInfo(addr, port, res)) return -1;

  int sockfd = -1;
  if ((sockfd = initSockCon(*res, &socketConnect)) == -1) {
    fprintf(stderr, "Failed to connect to socket\n");
    return -1;
  }

  setNonBlock(sockfd, 1);
  return sockfd;
}

int connection_ssl_client_init(const char *addr, const char *port, SSLConInfo *conInfo) {
  SSL_load_error_strings();
  SSL_library_init();
  conInfo->ctx = SSL_CTX_new(SSLv23_client_method());
  conInfo->enableSSL = 1;

  if (conInfo->ctx == NULL)
    ERR_print_errors_fp(stderr);

  fprintf(stderr, "Starting TCP Connection...\n");
  conInfo->socket = connection_client_init(addr, port, &conInfo->res);
  if (conInfo->socket < 0) return -1;

  fprintf(stderr, "Starting SSL Connection\n");
  conInfo->ssl = SSL_new(conInfo->ctx);
  if (!conInfo->ssl) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

  fprintf(stderr, "Binding SSL Connection\n");
  if (!SSL_set_fd(conInfo->ssl, conInfo->socket)) {
    ERR_print_errors_fp(stderr);
    return -1;
  }

  fprintf(stderr, "Setting connection state\n");
  SSL_set_connect_state(conInfo->ssl);
  int r = 0;
  int events = POLLIN | POLLOUT | POLLERR;
  conInfo->servfds.fd = conInfo->socket;

  while ((r = SSL_do_handshake(conInfo->ssl)) != 1) {
    int err = SSL_get_error(conInfo->ssl, r);
    if (err == SSL_ERROR_WANT_WRITE) {
      events |= POLLOUT;
      events &= ~POLLIN;
      fprintf(stderr, "Return want write set events %d\n", events);
    } else if (err == SSL_ERROR_WANT_READ) {
      events |= POLLIN;
      events &= ~POLLOUT;
      fprintf(stderr, "Return want read set events %d\n", events);
    } else {
      fprintf(stderr, "SSL_Do_handshake return %d error %d errno %d msg %s\n", r, err, errno, strerror(errno));
      ERR_print_errors_fp(stderr);
      return -1;
    }

    conInfo->servfds.events = events;
    do {
      r = poll(&conInfo->servfds, 1, 100);
    } while  (r == 0);

    if (r != 1) {
      fprintf(stderr, "poll return %d error events: %d errno %d %s\n", r, conInfo->servfds.revents, errno, strerror(errno));
      return -1;
    }
  }

  fprintf(stderr, "SSL Connection Successful!\n");
  return 0;
}

static int clientWrite(SSLConInfo *conInfo, char *buffer, size_t len) {
  if (!conInfo->enableSSL)
    return send(conInfo->servfds.fd, buffer, len, 0);

  return  SSL_write(conInfo->ssl, buffer, len);
}

int connection_client_send(SSLConInfo *conInfo, char *data, size_t len) {
  size_t total = 0, bytesLeft = len;
  int n = 0;
  while (total < len) {
    n = clientWrite(conInfo, data+total, bytesLeft);
    if (n == -1) break;
    total += n;
    bytesLeft -= n;
  }
  return n==-1?-1:0;
}


int connection_client_read(SSLConInfo *conInfo, char *buffer, size_t len) {
  if (!conInfo->enableSSL)
    return recv(conInfo->servfds.fd, buffer, len, 0);

  return SSL_read(conInfo->ssl, buffer, len);

  int rd = 0, r = 1;
  while (rd < len && r) {
    struct pollfd pfd = {};
    pfd.fd = conInfo->socket;
    pfd.events = POLLIN;
    r = poll(&pfd, 1, POLL_TIMEOUT_MS);
    if (r == 1) return 0;

    r = SSL_read(conInfo->ssl, buffer+rd, len - rd);
    if (r < 0) {
      int err = SSL_get_error(conInfo->ssl, r);
      if (err == SSL_ERROR_WANT_READ) {
        continue;
      } else if (err == SSL_ERROR_ZERO_RETURN) {
        //client disconnected
        return 0;
      }
      ERR_print_errors_fp (stderr);
    }
    rd += r;
  }
  return rd;
}

int connection_client_poll(SSLConInfo *conInfo, int event, int *ret) {
  if (conInfo->enableSSL) {
    *ret = 1;
    return 1;
  }
  return ((*ret = poll(&conInfo->servfds, 1, POLL_TIMEOUT_MS)) && conInfo->servfds.revents & event);
}



