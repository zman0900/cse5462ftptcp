#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "tcpd_interface.h"

typedef struct _sockinfo {
	const struct sockaddr *addr;
	socklen_t addrlen;
	int localport;
	int tcpdport;
	struct addrinfo *tcpdaddr;
} sockinfo;

char addrString[INET6_ADDRSTRLEN];
sockinfo *si = NULL;
int serverMode = 0;
char clientAckBuf[CLIENT_ACK_MSG_LEN];
// This is needed since recvfrom discards data beyond the requested size
// tcpd could send up to BUFFER_SIZE bytes of data at once, but ftps can only
// request up to 1000 bytes at once, so store it here.
char recvBuf[BUFFER_SIZE+1000];
int recvStart = 0;
int recvSize = 0;

//// PUBLIC FUNCTIONS ////

int SOCKET(int domain, int type, int protocol) {
	// Ignore input, create udp ipv4 socket
	return socket(AF_INET, SOCK_DGRAM, 0);
}

int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	struct addrinfo *servinfo, *p;

	if (((struct sockaddr_in *)addr)->sin_addr.s_addr
		     == htonl(INADDR_ANY)) {
		printf("tcpd_interface: Server mode\n");
		serverMode = 1;
	}

	// Store provided info for later
	si = malloc(sizeof(sockinfo));
	si->addr = addr;
	si->addrlen = addrlen;
	// Choose ports for tcpd communication
	int remoteport = ntohs(((struct sockaddr_in *)addr)->sin_port);
	si->localport = serverMode ? LOCAL_PORT_RECEIVER : LOCAL_PORT_SENDER;
	si->tcpdport = serverMode ? TCPD_PORT_RECEIVER : TCPD_PORT_SENDER;

	// Bind socket for communication with tcpd
	char lp[6];
	sprintf(lp, "%d", si->localport);
	if (fillServInfo(NULL, lp, &servinfo) < 0) {
		return -1;
	}
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("tcpd_interface: bind");
			close(sockfd);
			continue;
		}
		// Socket bound
		break;
	}

	// Make sure socket is bound
	if (p == NULL)  {
		fprintf(stderr, "tcpd_interface: Failed to bind to an address.\n");
		return -1;
	}

	// Clean up
	freeaddrinfo(servinfo);

	// Get remote address
	getInAddrString(addr->sa_family, (struct sockaddr *)addr, addrString,
	                sizeof addrString);

	// Print port info
	printf("tcpd_interface: Ports:\n\tlocal\t%d\n\ttcpd\t%d\n\tremote\t%d\n",
	       si->localport, si->tcpdport, remoteport);

	// Prepare address info for tcpd destination
	char tp[6];
	sprintf(tp, "%d", si->tcpdport);
	struct addrinfo *tcpdaddr;
	if (fillServInfo("localhost", tp, &tcpdaddr) < 0) {
		fprintf(stderr, "tcpd_interface: Couldn't get address info for tcpd\n");
		return -1;
	}
	si->tcpdaddr = tcpdaddr;

	return 0;
}

int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	// Wait for start
	char clientStartBuf[CLIENT_START_MSG_LEN];
	int bytes;
	if ((bytes = recvfrom(sockfd, clientStartBuf, CLIENT_START_MSG_LEN, 0,
	                      si->tcpdaddr->ai_addr,
		                  &(si->tcpdaddr->ai_addrlen))) < 0) {
		perror("tcpd_interface: ACCEPT");
		return -1;
	}
	if (bytes != CLIENT_START_MSG_LEN
	      || 0 != strcmp(CLIENT_START_MSG, clientStartBuf)) {
		fprintf(stderr,
		        "tcpd_interface: invalid packet received for client start\n");
		return -1;
	}
	printf("tcpd_interface: Client start received\n");

	addr = si->addr;
	addrlen = &(si->addrlen);

	return 0;
}

int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	return 0; // NO-OP
}

ssize_t SEND(int sockfd, const void *buf, size_t len, int flags) {
	// Check if open
	if (si == NULL) {
		// Socket not open
		fprintf(stderr, "tcpd_interface: attempt to send to non-open socket\n");
		return -1;
	}

	// Send all the bytes
	/*if (sendAllTo(sockfd, buf, (int *)&len, si->tcpdaddr->ai_addr,
		           si->tcpdaddr->ai_addrlen) < 0) {
		perror("tcpd_interface: SEND");
		return -1;
	}*/
	int total = 0;
	int bytesleft = len;
	int n, bytes;
	while (total < len) {
		if ((n = sendto(sockfd, buf+total, bytesleft, 0, si->tcpdaddr->ai_addr,
		                si->tcpdaddr->ai_addrlen)) < 0) {
			perror("send");
			break;
		}
		total += n;
		bytesleft -= n;
		// Wait for ack
		if ((bytes = recvfrom(sockfd, clientAckBuf, CLIENT_ACK_MSG_LEN, 0,
		                      si->tcpdaddr->ai_addr,
			                  &(si->tcpdaddr->ai_addrlen))) < 0) {
			perror("tcpd_interface: SEND (recv ack)");
			return -1;
		}
		if (bytes != CLIENT_ACK_MSG_LEN
		      || 0 != strcmp(CLIENT_ACK_MSG, clientAckBuf)) {
			fprintf(stderr,
			        "tcpd_interface: invalid packet received for client ack\n");
			return -1;
		}
		printf("tcpd_interface: Client ack received\n");
	}

	printf("tcpd_interface: Sent %d bytes\n", (int)len);

	return len;
}

ssize_t RECV(int sockfd, void *buf, size_t len, int flags) {
	// Check if open
	if (si == NULL) {
		// Socket not open
		fprintf(stderr, "tcpd_interface: attempt to recv from non-open socket\n");
		return -1;
	}

	if (recvSize > 0 && recvSize < len) {
		// Move to beginning of buffer
		memmove(recvBuf, recvBuf+recvStart, recvSize);
	}

	while (recvSize < len) {
		// Need more data
		int bytes;
		if ((bytes = recvfrom(sockfd, recvBuf+recvStart, BUFFER_SIZE, 0,
			                  si->tcpdaddr->ai_addr,
			                  &(si->tcpdaddr->ai_addrlen))) < 0) {
			perror("tcpd_interface: RECV");
			return -1;
		}
		recvSize += bytes;
		printf("tcpd_interface: Received %d bytes\n", bytes);
	}

	memcpy(buf, recvBuf+recvStart, len);
	recvSize -= len;
	if (recvSize == 0)
		recvStart = 0;
	else
		recvStart += len;
	printf("tcpd_interface: Delivered %lu bytes\n", len);
	return len;
}

int CLOSE(int fd) {
	// Get data
	if (si != NULL) {
		free(si);
		si = NULL;
		return close(fd);
	} else {
		// Socket not open
		fprintf(stderr, "tcpd_interface: attempt to close non-open socket\n");
		return close(fd);
	}
}

int recvBytes(int sockfd, char *buf, int *b) {
	int total = 0;
	int bytesleft = *b;
	int n;

	while (total < *b) {
		if ((n = RECV(sockfd, buf+total, bytesleft, 0)) == -1) {
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
