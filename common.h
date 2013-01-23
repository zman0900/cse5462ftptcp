#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#include <netinet/in.h>

#define FNAME_LEN 20

void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size);
int sendAll(int sockfd, char *buf, int *len);
int recvBytes(int sockfd, char *buf, int *b);

#endif
