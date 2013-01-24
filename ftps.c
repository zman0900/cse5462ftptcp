#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

#define CONNECTION_QUEUE 5
#define BUF_SIZE 1000
#define OUTPUT_DIR "ftps-out"

void receiveFile(int connfd) {
	char recvBuf[BUF_SIZE];
	int bytes;
	uint32_t fileSize, remaining, remainLastUpdate;
	char *fileName;
	FILE *file;
	time_t last, current, start;

	// Receive size
	bytes = 4;
	if (recvBytes(connfd, recvBuf, &bytes) != 0) exit(1);
	memcpy(&fileSize, recvBuf, 4);
	fileSize = ntohl(fileSize);

	// Receive name
	bytes = FNAME_LEN;
	if (recvBytes(connfd, recvBuf, &bytes) != 0) exit(1);
	fileName = malloc(strlen(recvBuf) + strlen(OUTPUT_DIR) + 2);
	// Prepend directory name
	strcpy(fileName, OUTPUT_DIR);
	strcat(fileName, "/");
	strcat(fileName, recvBuf);

	printf("Receiving file '%s' of size %u\n", fileName, fileSize);

	// Open file
	if ((file = fopen(fileName, "wb")) == NULL) {
		perror("Couldn't open file");
		exit(1);
	}

	// Receive and write file
	remainLastUpdate = remaining = fileSize;
	start = last = time(NULL);
	while (remaining > 0) {
		if ((bytes = recv(connfd, recvBuf,
		             (remaining < BUF_SIZE) ? remaining : BUF_SIZE, 0)) == -1) {
			perror("recv");
			exit(1);
		}
		if (bytes == 0) {
			fprintf(stderr, "Connection lost.\n");
			exit(2);
		}
		remaining -= bytes;
		// Display info only once per second
		current = time(NULL);
		if (current >= last + 1) {
			printf("Received %.02f KB (%.02f KB/s)...\n",
			       (fileSize-remainLastUpdate)/1000.0,
			       ((remainLastUpdate-remaining)/1000.0)/difftime(current, last));
			last = current;
			remainLastUpdate = remaining;
		}
		// Write to file
		if (fwrite(recvBuf, 1, bytes, file) != bytes) {
			fprintf(stderr, "File write error!\n");
			exit(1);
		}
	}

	// Show stats
	current = time(NULL);
	printf("Received %.02f KB (average %.02f KB/s)...\n", fileSize/1000.0,
	       (fileSize/1000.0)/difftime(current, start));

	fflush(file);
	fclose(file);
	printf("Done\n");
}

void sigchld_handler(int s) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
	printf("Child finished.\n");
}

int main(int argc, char *argv[]) {
	int sockfd, connfd;
	struct addrinfo hints, *servinfo, *p;
	int status, yes=1;
	struct sigaction sa;
	struct sockaddr_storage clientaddr;
	socklen_t ca_size;
	char addrString[INET6_ADDRSTRLEN];

	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		return 1;
	}
	printf("Starting server on port %s...\n", argv[1]);

	// Create output folder
	if (mkdir(OUTPUT_DIR, S_IRWXU) == -1) {
		int err = errno;
		if (err != EEXIST) {
			fprintf(stderr, "Error creating output directory: %s\n", strerror(err));
			exit(1);
		}
	}

	// Setup structures
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((status = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 1;
	}

	//Bind to first address
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		// Let other sockets bind port unless active listening socket
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		// Bind socket
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("bind");
			close(sockfd);
			continue;
		}
		// Socket bound
		break;
	}

	// Make sure socket is bound
	if (p == NULL)  {
		fprintf(stderr, "Failed to bind to an address.\n");
		return 1;
	}

	// Clean up
	freeaddrinfo(servinfo);

	// Start listening
	if (listen(sockfd, CONNECTION_QUEUE) == -1) {
		perror("listen");
		exit(1);
	}
	printf("Listening for connections...(Ctrl-C to stop)\n");

	// Zombie hunting
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	// Listen forever
	while(1) {
		ca_size = sizeof clientaddr;
		connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &ca_size);
		if (connfd == -1) {
			perror("accept");
			continue;
		}
		getInAddrString(clientaddr.ss_family, (struct sockaddr *)&clientaddr, 
			addrString, sizeof addrString);
		printf("New connection from %s\n", addrString);

		int pid;
		if ((pid = fork()) < 0) {
			perror("fork");
			exit(1);
		} else if (pid == 0) {
			// Child process
			// Close listener in child
			close(sockfd);
			receiveFile(connfd);
			exit(0);
		} else {
			// Parent process
			printf("Started child %d\n", pid);
			// Close connection in parent
			close(connfd);
		}
	}

	return 0;
}
