#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
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
#include "pktinfo.h"
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
static const int clock_g = 20000; // Clock granularity (microsec)
static const int minrto = 1000000; // Minimum RTO on 1 sec (rfc2988)

// Globals
int isSenderSide;
int socklocal, socklisten, socktimer;
struct addrinfo *trolladdr, *clientaddr;
char recvBuf[TCP_HEADER_SIZE+MSS], sendBuf[TCP_HEADER_SIZE+MSS];
int sendBufSize;
char addrString[INET6_ADDRSTRLEN];

// TCP State
int tcp_isConn = 0;
char buffer[BUFFER_SIZE];
uint32_t win_start = 0;
uint32_t send_next = 0; // Not used for receiving
uint32_t data_end = 0;  // Not used for receiving
int rto_set = 0;
uint32_t rto = 3000000; // Start at 3 seconds (rfc2988)
double srtt;
double rttvar;

// Temp storage for incoming data from client until buffer space is available
char waitingPkt[MSS];
int waitingPktSize;

// Functions
void bindLocal();
void bindListen();
void listenToPorts();
void preExit();
void recvClientMsg();
void storeInSendBufferAndAckClient(char *buf, int len);
void recvTcpMsg();
void updateRTO(uint32_t rtt);
void storeInRecvBuffer(char *buf, int len, uint32_t seqnum, uint32_t tsval);
void sendNextTcpPacket();
void sendTcpPacket(uint32_t seqnum, int pktSize);
void sendTcpAck(uint32_t acknum, uint32_t tsecr);
void timerExpired();
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
		// Set up timer
		char p[6];
		sprintf(p, "%d", randomPort());
		socktimer = bindUdpSocket(NULL, p);
		if (socktimer <= 0) {
			fprintf(stderr, "timer-test: bind port failed\n");
			preExit();
			exit(1);
		}
	}

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
		if (isSenderSide) FD_SET(socktimer, &readfds);

		// Block until input on a socket
		if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0) {
			perror("tcpd: select");
			preExit();
			exit(1);
		}
		printf("tcpd: woke up from select\n");

		if (isSenderSide && FD_ISSET(socktimer, &readfds)) {
			timerExpired();
		}

		if (FD_ISSET(socklocal, &readfds)) {
			// Msg from client
			recvClientMsg();
		}

		if (FD_ISSET(socklisten, &readfds)) {
			// Msg from other tcpd
			recvTcpMsg();
		}

		// Send data
		if (isSenderSide) {
			// Send tcp packets if data available and space in rwin
			sendNextTcpPacket();
		} else {
			// TODO: Send available data to client
			
		}
	}
}

void preExit() {
	
}

int getAvailableSpaceInSendBuffer() {
	int available;
	if (data_end >= win_start) {
		available = (win_start + BUFFER_SIZE) - data_end;
	} else {
		available = win_start - data_end;
	}
	return available;
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
	int available = getAvailableSpaceInSendBuffer();

	// Handle case of not enough space
	if (available < bytes) {
		printf("tcpd: Buffer full!\n");
		memcpy(waitingPkt, recvBuf, bytes);
		waitingPktSize = bytes;
		return;
	}

	// Store data in circular buffer
	storeInSendBufferAndAckClient(recvBuf, bytes);
}

void storeInSendBufferAndAckClient(char *buf, int len) {
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
	printf("tcpd: TcpMsg: Received %d bytes\n", bytes-TCP_HEADER_SIZE);
	Header *h = (Header *)recvBuf;

	// Verify checksum
	if (!tcpheader_verifycrc(recvBuf, bytes)) {
		printf("tcpd: CHECKSUM FAILED! seq?:%u ack?:%u DROPPED\n",
		       ntohl(h->field.seqnum), ntohl(h->field.acknum));
		return; // Drop it
	}
	printf("tcpd: Checksum OK seq:%u ack:%u\n", ntohl(h->field.seqnum),
	       ntohl(h->field.acknum));

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
		// TODO: Send ACK for FIN
	} else if (tcpheader_isack(h)) {
		uint32_t acknum = ntohl(h->field.acknum);
		printf("tcpd: Received ACK: %d\n", acknum);
		// Sender: Move send window
		if (acknum > win_start) {
			win_start = acknum;
			printf("tcpd: Window left edge is now: %d\n", win_start);
		}
		// Cancel any pending timers
		PktInfo *i = pktinfo_removeOneLessThan(acknum);
		while (i != NULL) {
			printf("tcpd: Canceling timer for seqnum: %d\n", i->seqnum);
			timer_cancel(socktimer, i->seqnum);
			free(i);
			i = pktinfo_removeOneLessThan(acknum);
		}
		// Get RTT
		uint32_t then = ntohl(h->field.tsecr);
		if (then > 0) {
			uint32_t now = getTimestamp();
			if (now > then) { // Ignore if overflow
				uint32_t diff = now - then;
				printf("tcpd: Got new RTT: %d\n", diff);
				// Calculate new RTO
				updateRTO(diff);
			}
		}
		// Put possible pending data in buffer and unblock SEND
		if (waitingPktSize > 0) {
			int available = getAvailableSpaceInSendBuffer();
			if (available >= waitingPktSize) {
				printf("tcpd: STORED WAITING PACKET\n");
				storeInSendBufferAndAckClient(waitingPkt, waitingPktSize);
			}
		}
	} else {
		// Receiver: Store data in buffer
		storeInRecvBuffer(data, bytes-TCP_HEADER_SIZE, ntohl(h->field.seqnum),
		                  ntohl(h->field.tsval));
		// Send data to client TODO: remove this
		/*sendBufSize = bytes - TCP_HEADER_SIZE;
		memcpy(sendBuf, data, sendBufSize);
		sendToClient(sendBuf, sendBufSize);*/
	}
}

void updateRTO(uint32_t rtt) {
	if (rto_set) {
		rttvar = 0.75*rttvar + 0.25*fabs(srtt-rtt);
		srtt = 0.875*srtt + 0.125*rtt;
	} else {
		rto_set = 1;
		srtt = rtt;
		rttvar = rtt/2;
	}
	rto = srtt + MAX(clock_g, 4*rttvar);
	rto = MAX(rto, minrto);
	printf("tcpd: New RTO: %d\n", rto);
}

void storeInRecvBuffer(char *buf, int len, uint32_t seqnum, uint32_t tsval) {
	if (seqnum < win_start) {
		// Already ACK'd, must be duplicate, drop
		printf("tcpd: Already ACK'd duplicate received.\n");
		// ACK again to prevent more retransmissions
		sendTcpAck(win_start, 0);
		return;
	}
	if (seqnum+len > win_start+WINSIZE) {
		// Outside rwin, drop
		printf("tcpd: PACKET IS OUTSIDE RWIN, DROPPING\n");
		return;
	}
	if ((data_end >= send_next && seqnum+len > send_next+BUFFER_SIZE)
	           || (data_end < send_next && seqnum+len > send_next)) {
		// Would overwrite data not send to client yet, drop
		printf("tcpd: PACKET DROPPED FOR LACK OF SPACE\n");
		return;
	}

	int inOrder = 0;
	if (seqnum == data_end) {
		inOrder = 1;
	} else if (seqnum > data_end) {
		printf("tcpd: PACKET IS EARLY\n");
	} else {
		printf("tcpd: DUPLICATE UN-ACKd PACKET!\n");
	}
	// Insert packet
	int win_pos = seqnum % BUFFER_SIZE;
	int first = MIN(BUFFER_SIZE - win_pos, len);
	int second = len - first;
	printf("tcpd: inserting at %d\n", win_pos);
	memcpy(buffer+win_pos, buf, first);
	if (second > 0) {
		// Wraps around
		printf("tcpd: wrap around %d bytes\n", second);
		memcpy(buffer, buf+first, second);
	}
	int add;
	if (inOrder) {
		data_end += len;
		// Check if this packet filled a gap
		add = 0;
		int tmp = pktinfo_remove(seqnum+len);
		while (tmp != -1) {
			add += tmp;
			tmp = pktinfo_remove(seqnum+len+add);
		}
		data_end += add;
		if (add != 0) {
			printf("tcpd: moved data_end forward by %d\n", add);
		}
	} else {
		// Put in outstanding packets list
		pktinfo_add(seqnum, len);
	}

	// Send ACK
	if (inOrder && add == 0) {
		// Ack cooresponds to packet just received, send TSER
		sendTcpAck(data_end, tsval);
	} else {
		// Don't send TSER (send 0 instead)
		sendTcpAck(data_end, 0);
	}
}

void sendNextTcpPacket() {
	if (data_end <= send_next) {
		// no data in buffer
		printf("tcpd: No data to send!\n");
		return;
	}
	int rwin_used = pktinfo_length();
	int pktSize = data_end - send_next;
	pktSize = MIN(MSS, pktSize);
	// Don't send more than rwin
	pktSize = MIN(WINSIZE - rwin_used, pktSize);
	if (pktSize <= 0) {
		printf("tcpd: NOT SENDING, no space in rwin\n");
		return;
	}

	// Actually send
	sendTcpPacket(send_next, pktSize);

	// Increment seqnum
	send_next += pktSize;
	printf("tcpd: send_next is now %d\n", send_next);
}

void sendTcpPacket(uint32_t seqnum, int pktSize) {
	int send_pos = seqnum % BUFFER_SIZE;
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
	tcpheader_create(listenport, rmttrollport, seqnum, 0, 0, 0, 0, 0,
	                 packetData, pktSize, sendBuf);

	// Prepare timer
	struct timeval *timer = malloc(sizeof(struct timeval));
	timer->tv_sec = 0;
	timer->tv_usec = rto;
	while (timer->tv_usec >= 1000000) {
		timer->tv_usec -= 1000000;
		timer->tv_sec += 1;
	}

	// Keep packet lenght until ack'd incase needed for retransmission
	pktinfo_add(seqnum, pktSize);

	// Send to other tcpd through troll
	sendToTroll(sendBuf, TCP_HEADER_SIZE + pktSize);

	// Start timer
	if (timer_start(socktimer, timer, seqnum) < 0) {
		fprintf(stderr, "timer-test: Failed to start timer 1\n");
	}
	free(timer);
}

void sendTcpAck(uint32_t acknum, uint32_t tsecr) {
	printf("tcpd: Sending ACK: %d\n", acknum);
	tcpheader_create(listenport, rmttrollport, 0, acknum, 0, 1,
	                                 0, tsecr, NULL, 0, sendBuf);
	// Send to other tcpd through troll
	sendToTroll(sendBuf, TCP_HEADER_SIZE);
	// Move up rwin
	if (acknum > win_start) {
		win_start = acknum;
		printf("tcpd: Window left edge is now: %d\n", win_start);
	}
}

void timerExpired() {
	uint32_t seqnum = timer_getExpired(socktimer);
	printf("tcpd: Need to retransmit seqnum %d...\n", seqnum);
	int len = pktinfo_remove(seqnum);
	if (len < 0) {
		fprintf(stderr, "tcpd: Retransmit needed for packet not in buffer!\n");
		preExit();
		exit(1);
	}
	sendTcpPacket(seqnum, len);
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
	uint32_t seqnum = ntohl(((Header *)buf)->field.seqnum);
	uint32_t acknum = ntohl(((Header *)buf)->field.acknum);
	if (sendAllTo(socklocal, buf, &bufLen, trolladdr->ai_addr,
		           trolladdr->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
	printf("tcpd: TcpMsg: Sent %d bytes, seq:%u ack:%u\n", bufLen, seqnum,
	       acknum);
}
