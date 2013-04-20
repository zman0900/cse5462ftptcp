#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "tcpheader.h"
#include "timer_interface.h"

// Ports
int clientport;     // Port local ftps/ftpc uses
int localport;      // Listen to connections from local ftps/ftpc, send to troll
int listenport;     // Listen for connections from other troll
int trollport;      // Port our troll is listening on
int rmttrollport;   // Port troll sends to on other tcpd
char *remote_host;

// Constants
static const char *clientAck = CLIENT_ACK_MSG;
static const int clientAckLen = CLIENT_ACK_MSG_LEN;
static const char *clientStart = CLIENT_START_MSG;
static const int clientStartLen = CLIENT_START_MSG_LEN;

// Globals
int isSenderSide;
int socklocal, socklisten;
struct addrinfo *trolladdr, *clientaddr;
char recvBuf[TCP_HEADER_SIZE+MSS], sendBuf[TCP_HEADER_SIZE+MSS];
int sendBufSize;
char addrString[INET6_ADDRSTRLEN];

// TCP State
int tcp_isConn = 0;
uint32_t seqnum = 0;
char buffer[BUFFER_SIZE];
uint32_t win_start = 0;
uint32_t send_next = 0; // Not used for receiving
uint32_t data_end = 0;  // Not used for receiving

// Temp storage for incoming data from client until buffer space is available
char tempBuf[MSS];
int tempBufSize;

// Functions
void bindLocal();
void bindListen();
void listenToPorts();
void preExit();
void recvClientMsg();
void storeInBufferAndAckClient(char *buf, int len);
void recvTcpMsg();
void sendTcpMsg();
void sendToClient();
void sendToTroll();

int main(int argc, char *argv[]) {
	if (argc < 2 || argc > 3) {
		printf("Usage: %s <remote-port> ", argv[0]);
		printf("[<remote-host>]\n\n");
		printf("If remote-host is specified, will start as sender and attempt ");
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
			argc = 2;  // Started with INADDR_ANY as arg, assume receiver side
		}
	}

	if (argc == 2) {
		// Receiver side
		isSenderSide = 0;
		clientport = LOCAL_PORT_RECEIVER;
		localport = TCPD_PORT_RECEIVER;
		trollport = TROLL_PORT_RECEIVER;
		// Listen on "remote-port"
		listenport = atoi(argv[1]);
		// Troll's remote port and host will be set later after receiving first
		// connection (should be other tcpd's host and listenport)
		rmttrollport = -1;
		remote_host = NULL;  // Also set later
	} else {
		// Sender side
		isSenderSide = 1;
		clientport = LOCAL_PORT_SENDER;
		localport = TCPD_PORT_SENDER;
		trollport = TROLL_PORT_SENDER;
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

		// Send data
		if (isSenderSide) {
			// Send tcp packets if data available and space in rwin
			sendTcpMsg();
			// Put possible pending data in buffer and unblock SEND
			
		} else {
			// Send available data to client
			
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

	// Check space
	int available;
	if (data_end >= win_start) {
		available = (win_start + BUFFER_SIZE) - data_end;
	} else {
		available = win_start - data_end;
	}

	// Handle case of not enough space
	if (available < bytes) {
		printf("tcpd: Buffer full!\n");
		memcpy(tempBuf, recvBuf, bytes);
		tempBufSize = bytes;
		return;  // TODO: this probably isn't finished
	}

// TODO: remove this (for project)
	// delay sending ack so client slows down to avoid dropped packets
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 50000;
	select(0, NULL, NULL, NULL, &tv);

	// Store data in circular buffer
	storeInBufferAndAckClient(recvBuf, bytes);
}

void storeInBufferAndAckClient(char *buf, int len) {
	// Store data in circular buffer
	int insert = data_end % BUFFER_SIZE;
	int copyToEnd = MIN(len, BUFFER_SIZE - insert);
	printf("tcpd: inserting at %d\n", insert);
	memcpy(buffer+insert, buf, copyToEnd);
	if (copyToEnd < len) {
		// Wrap around to beginning
		printf("tcpd: wrap around %d bytes\n", len-copyToEnd);
		memcpy(buffer, buf+copyToEnd, len-copyToEnd);
	}
	data_end += len;
	// Tell client data is stored
	sendToClient(clientAck, clientAckLen);
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
	Header *h = (Header *)recvBuf;

	// Verify checksum
	if (!tcpheader_verifycrc(recvBuf, bytes)) {
		printf("tcpd: CHECKSUM FAILED! seq:%u\n", ntohl(h->field.seqnum));
	} else {
		printf("tcpd: Checksum OK seq:%u\n", ntohl(h->field.seqnum));
	}

	// Unwrap tcp
	char *data = recvBuf+TCP_HEADER_SIZE;

	// If this is a new connection
	if (!tcp_isConn) {
		// Figure out where to send replys to if receiver side
		if (!isSenderSide) {
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
			// Unblock ACCEPT
			sendToClient(clientStart, clientStartLen);
		}
		tcp_isConn = 1;
	}

	// Since this is only one-way for HW2, just close connection on FIN
	if (tcpheader_isfin(h)) {
		printf("tcpd: Got FIN packet\n");
		tcp_isConn = 0;
		if (!isSenderSide) {
			free(remote_host);
			rmttrollport = -1;
		}
	}

	// Send data to client
	sendBufSize = bytes - TCP_HEADER_SIZE;
	memcpy(sendBuf, data, sendBufSize);
	sendToClient(sendBuf, sendBufSize);
}

void sendTcpMsg() {
	if (data_end <= send_next) {
		// no data in buffer
		return;
	}
	int pktSize = MIN(MSS, data_end - send_next);
	int send_pos = send_next % BUFFER_SIZE;
	char *packetData;
	if (send_pos + pktSize > BUFFER_SIZE) {
		// Packet wraps around end of buffer
		int first = (send_pos + pktSize) - BUFFER_SIZE;
		int second = pktSize - first;
		// Using recvBuf as temp storage
		memcpy(recvBuf, buffer+send_pos, first);
		memcpy(recvBuf+first, buffer, second);
		packetData = recvBuf;
	} else {
		packetData = buffer+send_pos;
	}

	// Add tcp header
	// TODO: fill in properly
	Header *h = tcpheader_create(listenport, rmttrollport, data_end, 0, 0, 0,
	                             0, 0, packetData, pktSize, sendBuf);
	send_next += pktSize;

	// Send to other tcpd through troll
	sendToTroll(sendBuf, TCP_HEADER_SIZE + pktSize);
}

void sendToClient(char *buf, int bufLen) {
	if (sendAllTo(socklocal, buf, &bufLen, clientaddr->ai_addr,
		           clientaddr->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
	printf("tcpd: ClientMsg: Sent %d bytes\n", bufLen);
}

void sendToTroll(char *buf, int bufLen) {
	uint16_t seqnum = ntohl(((Header *)buf)->field.seqnum);
	if (sendAllTo(socklocal, buf, &bufLen, trolladdr->ai_addr,
		           trolladdr->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
	printf("tcpd: TcpMsg: Sent %d bytes, seq:%u\n", bufLen, seqnum);
}
