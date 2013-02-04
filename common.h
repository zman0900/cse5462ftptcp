#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#include <netdb.h>
#include <netinet/in.h>

#define FNAME_LEN 20

// Returns bound socket, or negative on fail
int bindUdpSocket(char *host, char *port);
void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size);
// Fills servinfo, or returns negative on fail
int fillServInfo(char *host, char *port, struct addrinfo **servinfo);
int sendAll(int sockfd, char *buf, int *len);
int recvBytes(int sockfd, char *buf, int *b);

#endif
