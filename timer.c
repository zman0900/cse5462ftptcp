#include <endian.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

#define PACKET_SIZE 21

/*
 * INPUT PACKET FORMAT (total 21 bytes, all unsigned)
 * time in seconds (8 bytes)
 * additional time in microseconds (8 bytes)
 * sequence number (4 bytes)
 * start(1)/cancel(0) (1 byte)
 */

/*
 * OUTPUT PACKET FORMAT (total 4 bytes, all unsigned)
 * sequence number (4 bytes)
 */

typedef struct Dlist Dlist;
struct Dlist {
	struct sockaddr_in *client;
	struct timeval *dtime;
	uint32_t seqnum;
	Dlist *next;
};

char recvBuf[PACKET_SIZE];
int socklisten;
Dlist *dlist_start = NULL;

void recvMsg();
void dlist_print();
void dlist_insert(Dlist *item);
// Returns NULL if no item found in list
// Port should be in network byte order
Dlist* dlist_remove(unsigned short port, uint32_t seqnum);
void dlist_updateTime(struct timeval *elapsed);

int main(int argc, char *argv[]) {
	fd_set readfds;
	struct timeval *timer, *starttime, *now, *elapsed;

	timer = NULL;
	starttime = malloc(sizeof(struct timeval));
	now = malloc(sizeof(struct timeval));
	elapsed = malloc(sizeof(struct timeval));

	// Port to string
	char p[6];
	sprintf(p, "%d", TIMER_PORT);

	// Bind port
	socklisten = bindUdpSocket(NULL, p);
	if (socklisten <= 0) {
		fprintf(stderr, "timer: bind listen port failed\n");
		exit(1);
	}

	printf("timer: starting select loop\n");
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(socklisten, &readfds);

		// Set timer
		if (dlist_start != NULL) {
			if (timer == NULL)
				timer = malloc(sizeof(struct timeval));
			timer->tv_sec = dlist_start->dtime->tv_sec;
			timer->tv_usec = dlist_start->dtime->tv_usec;
			printf("\ntimer: should wake up in %ld.%06ld sec\n", timer->tv_sec,
			       timer->tv_usec);
		} else {
			if (timer != NULL) {
				free(timer);
				timer = NULL;
			}
		}
		gettimeofday(starttime, NULL);

		// Block until input on socket or timeout
		if (select(FD_SETSIZE, &readfds, NULL, NULL, timer) < 0) {
			perror("timer: select");
			exit(1);
		}

		// Update time after unblocking
		gettimeofday(now, NULL);
		timersub(now, starttime, elapsed);
		printf("timer: woke up after %ld.%06ld sec\n", elapsed->tv_sec,
		       elapsed->tv_usec);
		dlist_updateTime(elapsed);

		// Keep track of time while working.  Also serves to "start" timer as
		// soon as message received without first processing it
		gettimeofday(starttime, NULL);

		if (FD_ISSET(socklisten, &readfds)) {
			// Wake up for add/remove timer
			recvMsg();
		} else {
			// (this shouldn't be null if there was a timer, but...)
			if (dlist_start != NULL) {
				if (dlist_start->dtime->tv_sec <= 0
				       && dlist_start->dtime->tv_usec <= 0) {
					// Timer expired
					printf("timer: TIMER EXPIRED port:%u seqnum:%u\n",
					       ntohs(dlist_start->client->sin_port),
					       dlist_start->seqnum);
					// Send message
					if (sendto(socklisten, &(dlist_start->seqnum), 4, 0, 
					       (struct sockaddr *)(dlist_start->client),
					       sizeof *(dlist_start->client)) < 0) {
						perror("timer: sendto");
					}
					// Remove item
					Dlist *tmp = dlist_start;
					dlist_start = tmp->next;
					free(tmp->dtime);
					free(tmp);
					// Print
					printf("timer: delta list after timer expire:\n");
					dlist_print();
				}
			}
		}

		// Update time after working
		gettimeofday(now, NULL);
		timersub(now, starttime, elapsed);
		printf("timer: finished work after %ld.%06ld sec\n", elapsed->tv_sec,
		       elapsed->tv_usec);
		dlist_updateTime(elapsed);
	}

	return 0;
}

void recvMsg() {
	int bytes;
	uint64_t sec, usec;
	uint32_t seqnum;
	uint8_t flag;

	struct sockaddr_in *senderaddr = malloc(sizeof(struct sockaddr_in));
	socklen_t saddr_sz = sizeof senderaddr;
	if ((bytes = recvfrom(socklisten, recvBuf, PACKET_SIZE, 0,
	                      (struct sockaddr *)senderaddr, &saddr_sz)) < 0) {
		perror("timer: recvfrom");
		exit(1);
	}
	printf("timer: Msg: Received %d bytes\n", bytes);
	if (bytes != PACKET_SIZE) {
		fprintf(stderr, "timer: BAD PACKET RECEIVED!\n");
	}
	memcpy(&sec, recvBuf, 8);
	memcpy(&usec, recvBuf+8, 8);
	memcpy(&seqnum, recvBuf+16, 4);
	memcpy(&flag, recvBuf+20, 1);

	if (flag) {
		// Start timer, create new dlist entry
		printf("timer: Starting for seqnum %u\n", ntohl(seqnum));
		Dlist *item = malloc(sizeof(Dlist));
		item->client = senderaddr;
		item->dtime = malloc(sizeof(struct timeval));
		item->dtime->tv_sec = be64toh(sec);
		item->dtime->tv_usec = be64toh(usec);
		item->seqnum = ntohl(seqnum);
		dlist_insert(item);
	} else {
		// Cancel timer
		printf("timer: Canceling for seqnum %u\n", ntohl(seqnum));
		Dlist *remove = dlist_remove(senderaddr->sin_port, ntohl(seqnum));
		if (remove == NULL) {
			fprintf(stderr, "timer: Attempted to cancel non-existing timer!\n");
		} else {
			// Free memory
			free(remove->dtime);
			free(remove);
		}
	}

	printf("timer: delta list after start or cancel:\n");
	dlist_print();
}

void dlist_print() {
	printf("--BEGIN CURRENT DELTA LIST--\n");
	Dlist *ptr = dlist_start;
	while (ptr != NULL) {
		printf("port:%u seqnum:%u time:%ld.%06ld\n",
		       ntohs(ptr->client->sin_port), ptr->seqnum,
		       ptr->dtime->tv_sec, ptr->dtime->tv_usec);
		ptr = ptr->next;
	}
	printf("--END CURRENT DELTA LIST--\n");
}

void dlist_insert(Dlist *item) {
	// Easy if list is empty
	if (dlist_start == NULL) {
		dlist_start = item;
		return;
	}

	Dlist *right = dlist_start;
	Dlist *left = NULL;
	struct timeval *total_left, *total_right, *new;

	total_left = malloc(sizeof(struct timeval));
	total_left->tv_sec = total_left->tv_usec = 0;
	total_right = malloc(sizeof(struct timeval));
	total_right->tv_sec = right->dtime->tv_sec;
	total_right->tv_usec = right->dtime->tv_usec;

	// Find correct position to insert
	while (timercmp(item->dtime, total_right, >)) {
		timeradd(total_left, right->dtime, total_left);
		left = right;
		right = left->next;
		if (right == NULL) break;
		timeradd(total_right, right->dtime, total_right);
	}

	// Update times
	new = malloc(sizeof(struct timeval));
	timersub(item->dtime, total_left, new);
	if (right != NULL)
		timersub(total_right, item->dtime, right->dtime);
	free(item->dtime);
	item->dtime = new;
	free(total_left);
	free(total_right);

	// Insert
	if (left == NULL) {
		// Inserting at beginning
		item->next = dlist_start;
		dlist_start = item;
	} else {
		// Insert between left and right
		left->next = item;
		item->next = right;
	}
}

Dlist* dlist_remove(unsigned short port, uint32_t seqnum) {
	Dlist *prev = NULL;
	Dlist *ptr = dlist_start;

	// Find
	while (ptr != NULL) {
		if (ptr->client->sin_port == port && ptr->seqnum == seqnum)
			break;
		prev = ptr;
		ptr = ptr->next;
	}

	// Remove
	if (ptr != NULL) {
		if (ptr->next != NULL) {
			// Need to update next's time
			timeradd(ptr->dtime, ptr->next->dtime, ptr->next->dtime);
		}
		if (prev == NULL) {
			// Removing first
			dlist_start = ptr->next;
		} else {
			prev->next = ptr->next;
		}
	}

	return ptr;
}

void dlist_updateTime(struct timeval *elapsed) {
	if (dlist_start == NULL)
		return;
	timersub(dlist_start->dtime, elapsed, dlist_start->dtime);
	// Prevent negative values
	if (dlist_start->dtime->tv_sec < 0 || dlist_start->dtime->tv_usec < 0) {
		dlist_start->dtime->tv_sec = 0;
		dlist_start->dtime->tv_usec = 0;
	}
	printf("timer: new head time: %ld.%06ld\n", dlist_start->dtime->tv_sec,
	       dlist_start->dtime->tv_usec);
}
