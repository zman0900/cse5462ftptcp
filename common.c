#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

int bindUdpSocket(char *host, char *port) {
	struct addrinfo *servinfo, *p;
	int sock;

	// Setup structures
	if (fillServInfo(host, port, &servinfo) < 0) {
		return -1;
	}

	// Bind to first address
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype,
		      p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		// Bind socket
		if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
			perror("bind");
			close(sock);
			continue;
		}
		// Socket bound
		break;
	}

	// Make sure socket is bound
	if (p == NULL)  {
		fprintf(stderr, "Failed to bind to an address.\n");
		return -1;
	}

	// Clean up
	freeaddrinfo(servinfo);
	return sock;
}

void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size) {
	const void *src;
	if (sa->sa_family == AF_INET) {
		src = &(((struct sockaddr_in*)sa)->sin_addr);
	} else {
		src = &(((struct sockaddr_in6*)sa)->sin6_addr);
	}
	inet_ntop(af, src, dst, size);
}

uint32_t getTimestamp() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

int fillServInfo(char *host, char *port, struct addrinfo **servinfo) {
	struct addrinfo hints;
	int status;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	if (host == NULL) {
		hints.ai_flags = AI_PASSIVE;
	}
	if ((status = getaddrinfo(host, port, &hints, servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return -1;
	}

	return 0;
}

int sendAll(int sockfd, const void *buf, int *len) {
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

int sendAllTo(int sockfd, const void *buf, int *len,
              const struct sockaddr *dest_addr, socklen_t dest_len) {
	int total = 0;
	int bytesleft = *len;
	int n;

	while (total < *len) {
		if ((n = sendto(sockfd, buf+total, bytesleft, 0, dest_addr,
		                dest_len)) == -1) {
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

int randCalled = 0;
int randomPort() {
	if (!randCalled) {
		// Initialize rand
		srand(time(NULL));
		randCalled = 1;
	}
	return rand() % 64512 + 1024;
}

int recvBytes(int sockfd, char *buf, int *b) {
	int total = 0;
	int bytesleft = *b;
	int n;
	
	while (total < *b) {
		if ((n = recv(sockfd, buf+total, bytesleft, 0)) == -1) {
			perror("recv");
			break;
		}
		if (n == 0) {
			fprintf(stderr, "Connection lost.\n");
			return -1;
		}
		total += n;
		bytesleft -= n;
	}
	
	// Pass back actual amount received
	*b = total;
	
	// -1 for fail, 0 for success
	return n == -1 ? -1 : 0;
}

