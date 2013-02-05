#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "tcpheader.h"

// Ports
int clientport;     // Listen to connections from local ftps/ftpc
int listenport;     // Listen for connections from other troll
int trollport;      // Port our troll is listening on
int localtrollport; // Port we use to send to troll
int rmttrollport;   // Port troll sends to on other tcpd
char *remote_host;

// Globals
int isClientSide;
int troll_pid = -1;
int sockclient, socktroll, socklisten;
struct addrinfo *trolladdr, *clientaddr;
char recvBuf[TCP_HEADER_SIZE+MSS], sendBuf[TCP_HEADER_SIZE+MSS];
int sendBufSize;
char addrString[INET6_ADDRSTRLEN];

// TCP State
int tcp_isConn = 0;

// Functions
void bindClient();
void bindListen();
void bindTroll();
void forkTroll();
void killTroll();
void listenToPorts();
void preExit();
int randomPort();
void recvClientMsg();
void recvTcpMsg();
void sendToClient();
void sendToTroll();

int main(int argc, char *argv[]) {
	if (argc < 3 || argc > 4) {
		printf("Usage: %s <local-port> <remote-port> [<remote-host>]\n\n",
		       argv[0]);
		printf("If remote-host is specified, will start troll and attempt ");
		printf("connection to tcpd\nlistening on remote-host:remote-port.\n");
		printf("Without remote-host, will listen on remote-port for ");
		printf("connection from remote\ntroll.\n");
		return 1;
	}
	printf("tcpd: Starting...\n");

	clientport = atoi(argv[1]);

	// For either client or server side, trollport and localtrollport can be
	// random since not used elsewhere
	if (argc == 3) {
		// Server side
		isClientSide = 0;
		// Listen on "remote-port"
		listenport = atoi(argv[2]);
		// Troll's remote port and host will be set later after receiving first
		// connection (should be other tcpd's host and listenport)
		rmttrollport = -1;
		remote_host = NULL;  // Also set later
	} else {
		// Client side
		isClientSide = 1;
		// Listen on random port, put that in tcp source field
		// Troll's remote port will be "remote-port"
		rmttrollport = atoi(argv[2]);
		remote_host = argv[3];
		do {
			listenport = randomPort();
		} while(listenport == rmttrollport || listenport == clientport);
	}
	// Generate trollport and localtrollport
	do {
		trollport = randomPort();
	} while (trollport == listenport || trollport == rmttrollport
	         || trollport == clientport);
	do {
		localtrollport = randomPort();
	} while (localtrollport == trollport || localtrollport == listenport
	         || localtrollport == rmttrollport || localtrollport == clientport);

	printf("tcpd: Ports:\n\tclient\t%d\n\tlisten\t%d\n\ttroll\t%d\n\tltroll\
\t%d\n\trtroll\t%d\n",
	       clientport, listenport, trollport, localtrollport, rmttrollport);

	// If client side, fork troll now, otherwise do it later
	if (argc == 4) {
		forkTroll();
	}

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
	bindClient();
	bindListen();
	bindTroll();

	// Start listening to client and listen ports
	listenToPorts();

	preExit();
	return 0;
}

void bindClient() {
	// Port to string
	char cp[6];
	sprintf(cp, "%d", clientport);

	sockclient = bindUdpSocket(NULL, cp);
	if (sockclient <= 0) {
		preExit();
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
		exit(1);
	}
}

void bindTroll() {
	// Port to string
	char tp[6];
	sprintf(tp, "%d", localtrollport);

	socktroll = bindUdpSocket(NULL, tp);
	if (socktroll <= 0) {
		preExit();
		exit(1);
	}
}

void forkTroll() {
	// Convert ports to strings
	char tp[6], rtp[6], ltp[6];
	sprintf(tp, "%d", trollport);
	sprintf(rtp, "%d", rmttrollport);
	sprintf(ltp, "%d", localtrollport);

	if ((troll_pid = vfork()) < 0) {
		perror("tcpd: vfork");
		exit(1);
	} else if (troll_pid == 0) {
		// Child
		execl("./troll", "troll", "-S", "localhost", "-b", ltp, "-C",
		      remote_host, "-a", rtp, tp, (char *)NULL);
		// Only returns on error
		perror("tcpd: execl");
		exit(1);
	} else {
		// Parent
		sleep(1);  // Make sure troll is ready
		printf("tcpd: Started troll (pid %d)\n", troll_pid);
	}
}

void killTroll() {
	if (troll_pid > 0) {
		// Try not to kill troll if its still working
		sleep(1);
		kill(troll_pid, SIGINT);
		troll_pid = -1;
	}
}

void listenToPorts() {
	fd_set readfds;

	printf("tcpd: starting select loop\n");

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(sockclient, &readfds);
		FD_SET(socklisten, &readfds);

		// Block until input on a socket
		if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0) {
			perror("tcpd: select");
			preExit();
			exit(1);
		}

		if (FD_ISSET(sockclient, &readfds)) {
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
	// Kill troll before exiting
	killTroll();
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

void recvClientMsg() {
	int bytes;
	struct sockaddr_in senderaddr;
	socklen_t saddr_sz = sizeof senderaddr;
	if ((bytes = recvfrom(sockclient, recvBuf, MSS, 0,
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
		// Figure out where to send replys to if server side and start troll
		if (!isClientSide) {
			// Set up some address info
			getInAddrString(senderaddr.sin_family,
			                (struct sockaddr *)&senderaddr, addrString,
			                sizeof addrString);
			// Fill in missing fields
			rmttrollport = ntohs(h->field.sport);
			remote_host = malloc(INET6_ADDRSTRLEN);
			strcpy(remote_host, addrString);
			//forkTroll();  // Not for HW2
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
			killTroll();
		}
	}

	// Send data to client
	memcpy(sendBuf, data, bytes);
	sendBufSize = bytes;
	sendToClient();
}

// Expects sendBuf and sendBufSize to be prefilled
void sendToClient() {
	if (sendto(sockclient, sendBuf, sendBufSize, 0, clientaddr->ai_addr,
		           clientaddr->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
}

// Expects sendBuf and sendBufSize to be prefilled
void sendToTroll() {
	if (sendto(socktroll, sendBuf, sendBufSize, 0, trolladdr->ai_addr,
		           trolladdr->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
}
