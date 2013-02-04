#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// Ports
int clientport;		// Listen to connections from local ftps/ftpc
int listenport;		// Listen for connections from other troll
int trollport;		// Port our troll is listening on
int localtrollport;	// Port we use to send to troll
int rmttrollport;		// Port troll uses to send to other tcpd

// Globals
int troll_pid = -1;

// Functions
void forkTroll();
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
	printf("Starting tcpd...\n");

	clientport = atoi(argv[1]);

	// For either client or server side, trollport and localtrollport can be
	// random since not used elsewhere
	if (argc == 3) {
		// Server side
		// Listen on "remote-port"
		// Troll's remote port will be set later from tcp packet source field
		listenport = atoi(argv[2]);
		rmttrollport = -1;
	} else {
		// Client side
		// Listen on random port, put that in tcp source field
		// Troll's remote port will be "remote-port"
		rmttrollport = atoi(argv[2]);
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

	printf("Ports:\n\tclient\t%d\n\tlisten\t%d\n\ttroll\t%d\n\tltroll\t%d\n\trtroll\t%d\n",
	       clientport, listenport, trollport, localtrollport, rmttrollport);

	// If client side, fork troll now, otherwise do it later
	if (argc == 4) {
		forkTroll();
	}

	// Kill troll before exiting
	if (troll_pid > 0) {
		kill(troll_pid, SIGINT);
	}

	return 0;
}

void forkTroll() {
	if ((troll_pid = vfork()) < 0) {
		perror("vfork");
		exit(1);
	} else if (troll_pid == 0) {
		// Child
		execl("./troll", "troll", "-S", "localhost", "-a", "1234", "-C", "localhost", "-b", "1235", "1236", (char *)NULL);
		// Only returns on error
		perror("execl");
		exit(1);
	} else {
		// Parent
		printf("Started troll (pid %d)\n", troll_pid);
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
