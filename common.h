#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#include <netdb.h>
#include <netinet/in.h>

#define FNAME_LEN 20
#define MSS 1000 //bytes
#define WINSIZE 20 //mss

/*
 * Returns bound socket, or negative on fail
 */
int bindUdpSocket(char *host, char *port);

/*
 * Gets incoming address string and puts it in dst, size is length of dst
 */
void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size);

/*
 * Fills servinfo, or returns negative on fail
 */
int fillServInfo(char *host, char *port, struct addrinfo **servinfo);

/*
 * Calls send repeatedly until len bytes have been sent from buf, or returns -1
 * on failure and sets len to number of bytes sent.
 */
int sendAll(int sockfd, char *buf, int *len);

/*
 * Calls recv repeatedly until b bytes have been received, or returns -1 on
 * failure and sets b to number of bytes received.
 */
int recvBytes(int sockfd, char *buf, int *b);

#endif
