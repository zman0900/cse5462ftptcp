#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "tcpheader.h"

// Ports
int clientport;     // Port local ftps/ftpc uses
int localport;      // Listen to connections from local ftps/ftpc, send to troll
int listenport;     // Listen for connections from other troll
int trollport;      // Port our troll is listening on
int rmttrollport;   // Port troll sends to on other tcpd
char *remote_host;

// Globals
int isClientSide;
int socklocal, socklisten;
struct addrinfo *trolladdr, *clientaddr;
char recvBuf[TCP_HEADER_SIZE+MSS], sendBuf[TCP_HEADER_SIZE+MSS];
int sendBufSize;
char addrString[INET6_ADDRSTRLEN];

// TCP State
int tcp_isConn = 0;

// Functions
void bindLocal();
void bindListen();
void listenToPorts();
void preExit();
void recvClientMsg();
void recvTcpMsg();
void sendToClient();
void sendToTroll();

int main(int argc, char *argv[]) {
	if (argc < 2 || argc > 3) {
		printf("Usage: %s <remote-port> ", argv[0]);
		printf("[<remote-host>]\n\n");
		printf("If remote-host is specified, will start as client and attempt ");
		printf("connection to tcpd\nlistening on remote-host:remote-port.\n");
		printf("Without remote-host, will listen on remote-port for ");
		printf("connection from remote\ntroll.\n");
		return 1;
	}
	printf("tcpd: Starting...\n");

	if (argc == 3) {
		struct addrinfo *testrmt;
		if (fillServInfo(argv[2], argv[1], &testrmt) < 0) {
			printf("tcpd: possible error\n");
		}
		if (((struct sockaddr_in *)testrmt->ai_addr)->sin_addr.s_addr
		     == htonl(INADDR_ANY)) {
			argc = 2;  // Started with INADDR_ANY as arg, assume server side
		}
	}

	// For either client or server side, trollport can be random since not used
	// elsewhere
	if (argc == 2) {
		// Server side
		isClientSide = 0;
		clientport = LOCAL_PORT_SERVER;
		localport = TCPD_PORT_SERVER;
		trollport = TROLL_PORT_SERVER;
		// Listen on "remote-port"
		listenport = atoi(argv[1]);
		// Troll's remote port and host will be set later after receiving first
		// connection (should be other tcpd's host and listenport)
		rmttrollport = -1;
		remote_host = NULL;  // Also set later
	} else {
		// Client side
		isClientSide = 1;
		clientport = LOCAL_PORT_CLIENT;
		localport = TCPD_PORT_CLIENT;
		trollport = TROLL_PORT_CLIENT;
		// Listen on 1 + remote port, put that in tcp source field
		// Troll's remote port will be "remote-port"
		rmttrollport = atoi(argv[1]);
		remote_host = argv[2];
		listenport = 1 + rmttrollport;
	}
	// Generate trollport without collsion

	// Print selected ports
	printf("tcpd: Ports:\n\tclient\t%d\n\tlocal\t%d\n\tlisten\t%d\n\ttroll\t%d\
\n\trtroll\t%d\n",
	       clientport, localport, listenport, trollport, rmttrollport);

	// Set up some address info
	char tp[6], cp[6];
	sprintf(tp, "%d", trollport);
	sprintf(cp, "%d", clientport);
	if (fillServInfo("localhost", tp, &trolladdr) < 0) {
		preExit();
		exit(1);
	}
	if (fillServInfo("localhost", cp, &clientaddr) < 0) {
		preExit();
		exit(1);
	}

	// Bind ports
	bindLocal();
	bindListen();

	// Start listening to client and listen ports
	listenToPorts();

	preExit();
	return 0;
}

void bindLocal() {
	// Port to string
	char cp[6];
	sprintf(cp, "%d", localport);

	socklocal = bindUdpSocket(NULL, cp);
	if (socklocal <= 0) {
		preExit();
		fprintf(stderr, "tcpd: bind local port failed\n");
		exit(1);
	}
}

void bindListen() {
	// Port to string
	char lp[6];
	sprintf(lp, "%d", listenport);

	socklisten = bindUdpSocket(NULL, lp);
	if (socklisten <= 0) {
		preExit();
		fprintf(stderr, "tcpd: bind listen port failed\n");
		exit(1);
	}
}

void listenToPorts() {
	fd_set readfds;

	printf("tcpd: starting select loop\n");

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(socklocal, &readfds);
		FD_SET(socklisten, &readfds);

		// Block until input on a socket
		if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0) {
			perror("tcpd: select");
			preExit();
			exit(1);
		}

		if (FD_ISSET(socklocal, &readfds)) {
			// Msg from client
			printf("tcpd: select got msg from client\n");
			recvClientMsg();
		}

		if (FD_ISSET(socklisten, &readfds)) {
			// Msg from other tcpd
			printf("tcpd: select got msg from tcpd\n");
			recvTcpMsg();
		}
	}
}

void preExit() {
	
}

void recvClientMsg() {
	int bytes;
	struct sockaddr_in senderaddr;
	socklen_t saddr_sz = sizeof senderaddr;
	if ((bytes = recvfrom(socklocal, recvBuf, MSS, 0,
	                      (struct sockaddr *)&senderaddr, &saddr_sz)) < 0) {
		perror("tcpd: recvfrom");
		preExit();
		exit(1);
	}
	printf("tcpd: ClientMsg: Received %d bytes\n", bytes);

	// Wrap with tcp
	Header *h = tcpheader_create(listenport, rmttrollport, 0, 0, 0, 0, 0, 0); // TODO: fill in properly
	memcpy(sendBuf, h, TCP_HEADER_SIZE);
	free(h);
	memcpy(sendBuf+TCP_HEADER_SIZE, recvBuf, bytes);
	sendBufSize = TCP_HEADER_SIZE + bytes;

	// Send to other tcpd through troll
	sendToTroll();
}

void recvTcpMsg() {
	int bytes;
	struct sockaddr_in senderaddr;
	socklen_t saddr_sz = sizeof senderaddr;
	if ((bytes = recvfrom(socklisten, recvBuf, TCP_HEADER_SIZE + MSS, 0,
	                      (struct sockaddr *)&senderaddr, &saddr_sz)) < 0) {
		perror("tcpd: recvfrom");
		preExit();
		exit(1);
	}
	printf("tcpd: TcpMsg: Received %d bytes\n", bytes);

	// Unwrap tcp
	Header *h = (Header *)recvBuf;
	char *data = recvBuf+TCP_HEADER_SIZE;

	// If this is a new connection
	if (!tcp_isConn) {
		// Figure out where to send replys to if server side
		if (!isClientSide) {
			// Set up some address info
			getInAddrString(senderaddr.sin_family,
			                (struct sockaddr *)&senderaddr, addrString,
			                sizeof addrString);
			// Fill in missing fields
			rmttrollport = ntohs(h->field.sport);
			remote_host = malloc(INET6_ADDRSTRLEN);
			strcpy(remote_host, addrString);
			printf("tcpd: New connection open from %s, reply port %d\n",
			        remote_host, rmttrollport);
		}
		tcp_isConn = 1;
	}

	// Since this is only one-way for HW2, just close connection on FIN
	if (tcpheader_isfin(h)) {
		printf("tcpd: Got FIN packet\n");
		tcp_isConn = 0;
		if (!isClientSide) {
			free(remote_host);
			rmttrollport = -1;
		}
	}

	// Send data to client
	sendBufSize = bytes - TCP_HEADER_SIZE;
	memcpy(sendBuf, data, sendBufSize);
	sendToClient();
}

// Expects sendBuf and sendBufSize to be prefilled
void sendToClient() {
	if (sendAllTo(socklocal, sendBuf, &sendBufSize, clientaddr->ai_addr,
		           clientaddr->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
	printf("tcpd: ClientMsg: Sent %d bytes\n", sendBufSize);
}

// Expects sendBuf and sendBufSize to be prefilled
void sendToTroll() {
	if (sendAllTo(socklocal, sendBuf, &sendBufSize, trolladdr->ai_addr,
		           trolladdr->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
	printf("tcpd: TcpMsg: Sent %d bytes\n", sendBufSize);
}
