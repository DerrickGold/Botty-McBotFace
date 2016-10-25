#ifndef __CONNECTION_H__
#define  __CONNECTION_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>


extern int getConnectionInfo(const char *addr, const char *port, struct addrinfo **results);

extern int socketConnect(int sockfd, struct addrinfo *res);

extern int initSockCon(struct addrinfo *res, int (*action)(int, struct addrinfo *));

extern int clientInit(const char *addr, const char *port, struct addrinfo **res);

extern int sendAll(int sockfd, char *data, size_t len);


#endif // __CONNECTION_H__
