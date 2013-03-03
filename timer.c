#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

#define RECV_BUF_SIZE 1000

typedef struct Dlist Dlist;
struct Dlist {
	struct sockaddr_in *client;
	struct timeval *dtime;
	char *data;
	int data_bytes;
	Dlist *next;
};

char recvBuf[RECV_BUF_SIZE];
int socklisten;
Dlist *dlist_start = NULL;

void recvMsg();

// Following is taken from gnu.org
/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */ 
int
timeval_subtract (result, x, y)
	struct timeval *result, *x, *y;
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	  tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

int main(int argc, char *argv[]) {
	fd_set readfds;
	struct timeval *timer, *starttime, *now, *diff, *diff2;

	timer = NULL;
	starttime = malloc(sizeof(struct timeval));
	now = malloc(sizeof(struct timeval));
	diff = malloc(sizeof(struct timeval));
	diff2 = malloc(sizeof(struct timeval));

	// Port to string
	char p[6];
	sprintf(p, "%d", TIMER_PORT);

	// Bind port
	socklisten = bindUdpSocket(NULL, p);
	if (socklisten <= 0) {
		fprintf(stderr, "timer: bind local port failed\n");
		exit(1);
	}

	printf("timer: starting select loop\n");
	while (1) {
		FD_ZERO(&readfds);
		FD_SET(socklisten, &readfds);

		// Set timer
		if (dlist_start != NULL) {
			timer = malloc(sizeof(struct timeval));
			timer->tv_sec = dlist_start->dtime->tv_sec;
			timer->tv_usec = dlist_start->dtime->tv_usec;
			printf("timer: should wake up in %ld.%06ld sec\n", timer->tv_sec,
			       timer->tv_usec);
		} else {
			timer = NULL;
		}
		gettimeofday(starttime, NULL);

		// Block until input on a socket

		if (select(FD_SETSIZE, &readfds, NULL, NULL, timer) < 0) {
			perror("timer: select");
			exit(1);
		}

		if (FD_ISSET(socklisten, &readfds)) {
			printf("timer: select got msg\n");
			recvMsg();
		}

		// Check time
		gettimeofday(now, NULL);
		timeval_subtract(diff, now, starttime);
		printf("timer: woke up after %ld.%06ld sec\n", diff->tv_sec,
		       diff->tv_usec);
		printf("timer: starttime: %ld.%06ld sec\n", starttime->tv_sec,
		       starttime->tv_usec);
		printf("timer: now: %ld.%06ld sec\n", now->tv_sec, now->tv_usec);
		if (timer != NULL) {
			int neg = timeval_subtract(diff2, dlist_start->dtime, diff);
			printf("timer: neg:%d diff:%ld.%06ld\n", neg, diff2->tv_sec,
			       diff2->tv_usec);
			if (neg || (diff2->tv_sec == 0 && diff2->tv_usec < 100)) {
				// Timer expired for first list item
				dlist_start = dlist_start->next;
			} else {
				// reset timer
				dlist_start->dtime = diff2;
			}
		}
	}

	return 0;
}

void recvMsg() {
	int bytes;
	struct sockaddr_in *senderaddr = malloc(sizeof(struct sockaddr_in));
	socklen_t saddr_sz = sizeof senderaddr;
	if ((bytes = recvfrom(socklisten, recvBuf, RECV_BUF_SIZE, 0,
	                      (struct sockaddr *)senderaddr, &saddr_sz)) < 0) {
		perror("timer: recvfrom");
		exit(1);
	}
	printf("timer: Msg: Received %d bytes\n", bytes);

	Dlist *item = malloc(sizeof(Dlist));
	item->client = senderaddr;
	item->data = malloc(bytes);
	memcpy(item->data, recvBuf, bytes);
	item->data_bytes = bytes;
	item->next = dlist_start;
	item->dtime = malloc(sizeof(struct timeval));
	item->dtime->tv_sec = 2;
	item->dtime->tv_usec = 100000;
	dlist_start = item;
}
