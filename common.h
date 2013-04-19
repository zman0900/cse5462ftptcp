#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>

#define FNAME_LEN 20
#define MSS 1000 //bytes
#define WINSIZE 20 //mss

#define LOCAL_PORT_SERVER 6660
#define LOCAL_PORT_CLIENT 6663
#define TCPD_PORT_SERVER 6661
#define TCPD_PORT_CLIENT 6664
#define TROLL_PORT_SERVER 6662
#define TROLL_PORT_CLIENT 6665
#define TIMER_PORT 6666

#define CLIENT_ACK_MSG "ack"
#define CLIENT_ACK_MSG_LEN 4

/*
 * Returns bound socket, or negative on fail
 */
int bindUdpSocket(char *host, char *port);

/*
 * Gets incoming address string and puts it in dst, size is length of dst
 */
void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size);

/*
 * Returns 32bit timestamp in microseconds, expect overflow
 */
uint32_t getTimestamp();

/*
 * Fills servinfo, or returns negative on fail
 */
int fillServInfo(char *host, char *port, struct addrinfo **servinfo);

/*
 * Calls send repeatedly until len bytes have been sent from buf, or returns -1
 * on failure and sets len to number of bytes sent.
 */
int sendAll(int sockfd, const void *buf, int *len);

/*
 * Calls sendto repeatedly until len bytes have been sent from buf, or returns
 * -1 on failure and sets len to number of bytes sent.
 */
int sendAllTo(int sockfd, const void *buf, int *len,
              const struct sockaddr *dest_addr, socklen_t dest_len);

/*
 * Returns a random int between 1024 and 65535 inclusive
 */
int randomPort();

#endif
