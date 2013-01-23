#include <arpa/inet.h>

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

int sendAll(int sockfd, char *buf, int *len) {
	int total = 0;
	int bytesleft = *len;
	int n;

	while (total < *len) {
		if ((n = send(sockfd, buf+total, bytesleft, 0)) == -1) {
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
