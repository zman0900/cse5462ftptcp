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
#include "tcpd_interface.h"

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

	printf("ftps: Receiving file '%s' of size %u\n", fileName, fileSize);

	// Open file
	if ((file = fopen(fileName, "wb")) == NULL) {
		perror("ftps: Couldn't open file");
		exit(1);
	}

	// Receive and write file
	remainLastUpdate = remaining = fileSize;
	start = last = time(NULL);
	while (remaining > 0) {
		if ((bytes = RECV(connfd, recvBuf,
		             (remaining < BUF_SIZE) ? remaining : BUF_SIZE, 0)) == -1) {
			perror("ftps: recv");
			exit(1);
		}
		if (bytes == 0) {
			fprintf(stderr, "ftps: Connection lost.\n");
			exit(2);
		}
		remaining -= bytes;
		// Display info only once per second
		current = time(NULL);
		if (current >= last + 1) {
			printf("ftps: Received %.02f KB (%.02f KB/s)...\n",
			       (fileSize-remainLastUpdate)/1000.0,
			       ((remainLastUpdate-remaining)/1000.0)/difftime(current, last));
			last = current;
			remainLastUpdate = remaining;
		}
		// Write to file
		if (fwrite(recvBuf, 1, bytes, file) != bytes) {
			fprintf(stderr, "ftps: File write error!\n");
			exit(1);
		}
	}

	// Show stats
	current = time(NULL);
	printf("ftps: Received %.02f KB (average %.02f KB/s)...\n", fileSize/1000.0,
	       (fileSize/1000.0)/difftime(current, start));

	fflush(file);
	//fclose(file);  // This is causing a segfault now, but not in gdb...
	printf("ftps: Done\n");
}

/*void sigchld_handler(int s) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
	printf("ftps: Child finished.\n");
}*/

int main(int argc, char *argv[]) {
	int sockfd, connfd;
	struct addrinfo hints, *servinfo;
	int status;
	//struct sigaction sa;
	//struct sockaddr_storage clientaddr;
	//socklen_t ca_size;
	//char addrString[INET6_ADDRSTRLEN];

	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		return 1;
	}
	printf("ftps: Starting server on port %s...\n", argv[1]);

	// Create output folder
	if (mkdir(OUTPUT_DIR, S_IRWXU) == -1) {
		int err = errno;
		if (err != EEXIST) {
			fprintf(stderr, "ftps: Error creating output directory: %s\n",
			        strerror(err));
			exit(1);
		}
	}

	// Setup structures
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((status = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "ftps: getaddrinfo: %s\n", gai_strerror(status));
		return 1;
	}

	// Get socket
	if ((sockfd = SOCKET(servinfo->ai_family, servinfo->ai_socktype,
	                     servinfo->ai_protocol)) == -1) {
		perror("ftps: socket");
		exit(1);
	}
	// Bind
	if (BIND(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		perror("ftps: bind");
		CLOSE(sockfd);
		exit(1);
	}

	// Clean up
	freeaddrinfo(servinfo);

	// Start listening
	/*if (listen(sockfd, CONNECTION_QUEUE) == -1) {
		perror("ftps: listen");
		exit(1);
	}
	printf("ftps: Listening for connections...(Ctrl-C to stop)\n");*/

	// Zombie hunting
	/*sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("ftps: sigaction");
		exit(1);
	}*/

	// Listen forever
	/*while(1) {
		ca_size = sizeof clientaddr;
		connfd = ACCEPT(sockfd, (struct sockaddr *)&clientaddr, &ca_size);
		if (connfd == -1) {
			perror("ftps: accept");
			continue;
		}
		getInAddrString(clientaddr.ss_family, (struct sockaddr *)&clientaddr, 
			addrString, sizeof addrString);
		printf("ftps: New connection from %s\n", addrString);

		int pid;
		if ((pid = fork()) < 0) {
			perror("fork");
			exit(1);
		} else if (pid == 0) {
			// Child process
			// Close listener in child
			close(sockfd);*/
			//receiveFile(connfd);
			receiveFile(sockfd);
			CLOSE(sockfd);
			exit(0);
		/*} else {
			// Parent process
			printf("ftps: Started child %d\n", pid);
			// Close connection in parent
			close(connfd);
		}
	}*/

	return 0;
}
