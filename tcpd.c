#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

// Ports
int clientport;     // Listen to connections from local ftps/ftpc
int listenport;     // Listen for connections from other troll
int trollport;      // Port our troll is listening on
int localtrollport; // Port we use to send to troll
int rmttrollport;   // Port troll sends to on other tcpd
char *remote_host;

// Globals
int troll_pid = -1;
int sockclient, socktroll, socklisten;

// Functions
void bindClient();
void bindListen();
void bindTroll();
void forkTroll();
void preExit();
int randomPort();

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
		// Listen on "remote-port"
		// Troll's remote port will be set later from tcp packet source field
		listenport = atoi(argv[2]);
		rmttrollport = -1;
		remote_host = NULL;  // Also set later
	} else {
		// Client side
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

	// Bind ports
	bindClient();
	bindListen();
	bindTroll();

	// Test send to troll
	struct addrinfo *servinfo;
	char tp[6];
	sprintf(tp, "%d", trollport);
	if (fillServInfo("localhost", tp, &servinfo) < 0) {
		preExit();
		exit(1);
	}
	if (sendto(socktroll, "test", 5, 0, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
		perror("tcpd: sendto");
		preExit();
		exit(1);
	}
	// End test

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
		execl("./troll", "troll", "-S", "localhost", "-a", ltp, "-C",
		      remote_host, "-b", rtp, tp, (char *)NULL);
		// Only returns on error
		perror("tcpd: execl");
		exit(1);
	} else {
		// Parent
		sleep(1);  // Make sure troll is ready
		printf("tcpd: Started troll (pid %d)\n", troll_pid);
	}
}

void preExit() {
	// Kill troll before exiting
	if (troll_pid > 0) {
		// Try not to kill troll if its still working
		sleep(1);
		kill(troll_pid, SIGINT);
	}
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
