#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>

#include "common.h"

void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size) {
	const void *src;
	if (sa->sa_family == AF_INET) {
		src = &(((struct sockaddr_in*)sa)->sin_addr);
	} else {
		src = &(((struct sockaddr_in6*)sa)->sin6_addr);
	}
	inet_ntop(af, src, dst, size);
}

/*
 * Calls send repeatedly until len bytes have been sent from buf, or returns -1
 * on failure and sets len to number of bytes sent.
 */
int sendAll(int sockfd, char *buf, int *len) {
	int total = 0;
	int bytesleft = *len;
	int n;

	while (total < *len) {
		if ((n = send(sockfd, buf+total, bytesleft, 0)) == -1) {
			perror("send");
			break;
		}
		total += n;
		bytesleft -= n;
	}

	// Pass back actual amount sent
	*len = total;

	// -1 for fail, 0 success
	return n == -1 ? -1 : 0;
}

/*
 * Calls recv repeatedly until b bytes have been received, or returns -1 on
 * failure and sets b to number of bytes received.
 */
int recvBytes(int sockfd, char *buf, int *b) {
	int total = 0;
	int bytesleft = *b;
	int n;
	
	while (total < *b) {
		if ((n = recv(sockfd, buf+total, bytesleft, 0)) == -1) {
			perror("recv");
			break;
		}
		total += n;
		bytesleft -= n;
	}
	
	// Pass back actual amount received
	*b = total;
	
	// -1 for fail, 0 for success
	return n == -1 ? -1 : 0;
}

