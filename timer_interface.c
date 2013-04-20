#include <endian.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "timer_interface.h"

struct addrinfo *timeraddr = NULL;

void initAddress() {
	if (timeraddr == NULL) {
		char p[6];
		sprintf(p, "%d", TIMER_PORT);
		if (fillServInfo("localhost", p, &timeraddr) < 0) {
			fprintf(stderr, "timer_interface: Couldn't set up address!\n");
			exit(1);
		}
	}
}

int timer_start(const int socket, const struct timeval * const time,
	            const uint32_t _seqnum) {
	char *sendBuf = malloc(PACKET_SIZE);
	uint64_t sec = htobe64(time->tv_sec);
	uint64_t usec = htobe64(time->tv_usec);
	uint32_t seqnum = htonl(_seqnum);
	uint8_t flag = 1;

	// Build packet
	memcpy(sendBuf, &sec, 8);
	memcpy(sendBuf+8, &usec, 8);
	memcpy(sendBuf+16, &seqnum, 4);
	memcpy(sendBuf+20, &flag, 1);

	initAddress();
	if (sendto(socket, sendBuf, PACKET_SIZE, 0, timeraddr->ai_addr,
	           timeraddr->ai_addrlen) != PACKET_SIZE) {
		// If packet isn't sent in full, just fail
		return -1;
	}

	return 0;
}

int timer_cancel(const int socket, const uint32_t _seqnum) {
	char *sendBuf = malloc(PACKET_SIZE);
	uint32_t seqnum = htonl(_seqnum);
	uint8_t flag = 0;

	// Build packet, zero timer fields
	memset(sendBuf, 0, PACKET_SIZE);
	memcpy(sendBuf+16, &seqnum, 4);
	memcpy(sendBuf+20, &flag, 1);

	initAddress();
	if (sendto(socket, sendBuf, PACKET_SIZE, 0, timeraddr->ai_addr,
	           timeraddr->ai_addrlen) != PACKET_SIZE) {
		// If packet isn't sent in full, just fail
		return -1;
	}

	return 0;
}

uint32_t timer_getExpired(const int socket) {
	uint32_t seqnum;
	if (recvfrom(socket, &seqnum, 4, 0, timeraddr->ai_addr,
	      &(timeraddr->ai_addrlen)) != 4) {
		fprintf(stderr, "timer_interface: Invalid response from timer!\n");
	}
	return ntohl(seqnum);
}
