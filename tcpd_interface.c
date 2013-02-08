#include <arpa/inet.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "tcpd_interface.h"

GHashTable *sockets = NULL;
char addrString[INET6_ADDRSTRLEN];

typedef struct _sockinfo {
	const struct sockaddr *addr;
	socklen_t addrlen;
	int localport;
	int tcpdport;
	pid_t tcpd_pid;
} sockinfo;

void initHashTable() {
	sockets = g_hash_table_new(g_int_hash, g_int_equal);
}

pid_t forkTcpd(int clientport, int localport, int remoteport, char *host) {
	// Convert ports to strings
	char cp[6], lp[6], rp[6];
	sprintf(cp, "%d", clientport);
	sprintf(lp, "%d", localport);
	sprintf(rp, "%d", remoteport);

	pid_t tcpd_pid;
	if ((tcpd_pid = vfork()) < 0) {
		perror("tcpd_interface: vfork");
		exit(1);
	} else if (tcpd_pid == 0) {
		// Child
		execl("./tcpd", "tcpd", cp, lp, rp, host, 
		      (char *)NULL);
		// Only returns on error
		perror("tcpd_interface: execl");
		exit(1);
	} else {
		// Parent
		printf("tcpd_interface: Started tcpd (pid %d)\n", tcpd_pid);
	}
	return tcpd_pid;
}

//// PUBLIC FUNCTIONS ////

int SOCKET(int domain, int type, int protocol) {
	// Ignore input, create udf ipv4 socket
	return socket(AF_INET, SOCK_DGRAM, 0);
}

int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	if (sockets == NULL) {
		initHashTable();
	}

	// Store provided info for later
	sockinfo *si = malloc(sizeof(sockinfo));
	si->addr = addr;
	si->addrlen = addrlen;
	// Choose ports for tcpd communication
	int remoteport = ntohs(((struct sockaddr_in *)addr)->sin_port);
	do {
		si->localport = randomPort();
	} while (remoteport == si->localport);
	do {
		si->tcpdport = randomPort();
	} while (remoteport == si->tcpdport || si->localport == si->tcpdport);

	// Bind socket for communication with tcpd
	char lp[6];
	sprintf(lp, "%d", si->localport);
	int socket = bindUdpSocket(NULL, lp);

	// Get remote address
	getInAddrString(addr->sa_family, (struct sockaddr *)addr, addrString,
	                sizeof addrString);

	// Fork tcpd using provided info
	printf("tcpd_interface: Ports:\n\tlocal\t%d\n\ttcpd\t%d\n\tremote\t%d\n",
	       si->localport, si->tcpdport, remoteport);
	si->tcpd_pid = forkTcpd(si->localport, si->tcpdport, remoteport, addrString);

	// Store in hash table for other functions
	g_hash_table_replace(sockets, &socket, si);

	return socket;
}

int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
	return 0; // TODO: Implement (for project)
}

int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	return 0; // NO-OP
}

ssize_t SEND(int sockfd, const void *buf, size_t len, int flags) {
	return -666;
}

ssize_t RECV(int sockfd, void *buf, size_t len, int flags) {
	return -666;
}

int CLOSE(int fd) {
	return -666;
}
