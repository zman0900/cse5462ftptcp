#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "timer_interface.h"

int timer_socket;

int main(int argc, char *argv[]) {
	struct timeval *timer;

	// Port to string
	char p[6];
	sprintf(p, "%d", randomPort());

	// Bind port
	timer_socket = bindUdpSocket(NULL, p);
	if (timer_socket <= 0) {
		fprintf(stderr, "timer-test: bind port failed\n");
		exit(1);
	}

	timer = malloc(sizeof(struct timeval));

	timer->tv_sec = 20;
	timer->tv_usec = 0;
	if (timer_start(timer_socket, timer, 1) < 0) {
		fprintf(stderr, "timer-test: Failed to start timer 1\n");
	} else {
		printf("Started timer 1\n");
	}

	timer->tv_sec = 10;
	timer->tv_usec = 0;
	if (timer_start(timer_socket, timer, 2) < 0) {
		fprintf(stderr, "timer-test: Failed to start timer 2\n");
	} else {
		printf("Started timer 2\n");
	}

	timer->tv_sec = 30;
	timer->tv_usec = 0;
	if (timer_start(timer_socket, timer, 3) < 0) {
		fprintf(stderr, "timer-test: Failed to start timer 3\n");
	} else {
		printf("Started timer 3\n");
	}

	sleep(5);

	if (timer_cancel(timer_socket, 2) < 0) {
		fprintf(stderr, "timer-test: Failed to cancel timer 2\n");
	} else {
		printf("Canceled timer 2\n");
	}

	timer->tv_sec = 20;
	timer->tv_usec = 0;
	if (timer_start(timer_socket, timer, 4) < 0) {
		fprintf(stderr, "timer-test: Failed to start timer 4\n");
	} else {
		printf("Started timer 4\n");
	}

	sleep(5);

	timer->tv_sec = 18;
	timer->tv_usec = 0;
	if (timer_start(timer_socket, timer, 5) < 0) {
		fprintf(stderr, "timer-test: Failed to start timer 5\n");
	} else {
		printf("Started timer 5\n");
	}

	if (timer_cancel(timer_socket, 4) < 0) {
		fprintf(stderr, "timer-test: Failed to cancel timer 4\n");
	} else {
		printf("Canceled timer 4\n");
	}

	if (timer_cancel(timer_socket, 8) < 0) {
		fprintf(stderr, "timer-test: Failed to cancel timer 8\n");
	} else {
		printf("Canceled timer 8\n");
	}

	return 0;
}
