#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int randCalled = 0;
int randomPort() {
	if (!randCalled) {
		// Initialize rand
		srand(time(NULL));
		randCalled = 1;
	}
	return rand() % 64512 + 1024;
}

int main(int argc, char *argv[]) {
	int clientport;		// Listen to connections from local ftps/ftpc
	int listenport;		// Listen for connections from other troll
	int trollport;		// Port our troll is listening on
	int localtrollport;	// Port we use to send to troll
	int rmttrollport;		// Port troll uses to send to other tcpd

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

	return 0;
}
