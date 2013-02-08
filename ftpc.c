#include <arpa/inet.h>
#include <errno.h>
#include <libgen.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "tcpd_interface.h"

#define BUF_SIZE 1000

int sockfd;
FILE *file;
char *fileName, *filePath;
int fileNameLen;
uint32_t fileSize;

void openConnection(char *host, char *port) {
	struct addrinfo hints, *servinfo;
	int status;
	char addrString[INET6_ADDRSTRLEN];

	// Setup structures
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "ftpc: getaddrinfo: %s\n", gai_strerror(status));
		exit(1);
	}

	// Get socket
	if ((sockfd = SOCKET(servinfo->ai_family, servinfo->ai_socktype,
	                     servinfo->ai_protocol)) == -1) {
		perror("ftpc: socket");
		exit(1);
	}
	// Bind
	if (BIND(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		perror("ftpc: bind");
		CLOSE(sockfd);
		exit(1);
	}
	// Connect
	if (CONNECT(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
		perror("ftpc: connect");
		CLOSE(sockfd);
		exit(1);
	}

	getInAddrString(servinfo->ai_family, (struct sockaddr *)servinfo->ai_addr, 
			addrString, sizeof addrString);
	printf("ftpc: Connected to %s\n", addrString);

	// Clean up
	freeaddrinfo(servinfo);
}

void openFile() {
	struct stat st;

	if ((fileNameLen = strlen(fileName)) > FNAME_LEN - 1) {
		fprintf(stderr, "ftpc: File name must be less than %d characters!\n", 
		        FNAME_LEN);
		exit(1);
	}
	if ((file = fopen(filePath, "rb")) == NULL) {
		perror("ftpc: Couldn't open file");
		exit(1);
	}
	// Get size
	if (fstat(fileno(file), &st) == -1) {
		perror("ftpc: Can't determine size of file");
		exit(1);
	}
	// Make sure size fits in 32bit
	if (st.st_size > UINT32_MAX) {
		fprintf(stderr, "ftpc: File too large!\n");
		exit(1);
	}
	fileSize = st.st_size;
}

void sendFile() {
	char sendBuf[BUF_SIZE];
	uint32_t netSize = htonl(fileSize);
	int len;
	uint32_t remaining, remainLastUpdate;
	time_t last, current, start;

	// Send size
	memcpy(sendBuf, &netSize, 4);
	len = 4;
	if (SEND(sockfd, sendBuf, len, 0) <= 0) {
		fprintf(stderr, "ftpc: Send error!\n");
		CLOSE(sockfd);
		exit(1);
	}

	// Send name in 20 bytes
	memset(sendBuf, '\0', FNAME_LEN);  // Not really necessary...
	memcpy(sendBuf, fileName, fileNameLen + 1);
	len = FNAME_LEN;
	if (SEND(sockfd, sendBuf, len, 0) <= 0) {
		fprintf(stderr, "ftpc: Send error!\n");
		CLOSE(sockfd);
		exit(1);
	}

	// Read and send file
	remainLastUpdate = remaining = fileSize;
	start = last = time(NULL);
	while (remaining >= BUF_SIZE) {
		if (fread(sendBuf, 1, BUF_SIZE, file) != BUF_SIZE) {
			fprintf(stderr, "ftpc: File read error!\n");
			CLOSE(sockfd);
			exit(1);
		}
		len = BUF_SIZE;
		if (SEND(sockfd, sendBuf, len, 0) <= 0) {
			fprintf(stderr, "ftpc: Send error!\n");
			CLOSE(sockfd);
			exit(1);
		}
		remaining -= BUF_SIZE;
		// Display info only once per second
		current = time(NULL);
		if (current >= last + 1) {
			printf("ftpc: Sent %.02f KB (%.02f KB/s)...\n",
			       (fileSize-remainLastUpdate)/1000.0,
			       ((remainLastUpdate-remaining)/1000.0)/difftime(current, last));
			last = current;
			remainLastUpdate = remaining;
		}
	}
	// Send any remaining less buffer size
	if (fread(sendBuf, 1, remaining, file) != remaining) {
		fprintf(stderr, "ftpc: File read error!\n");
	}
	len = remaining;
	if (SEND(sockfd, sendBuf, len, 0) <= 0) {
		fprintf(stderr, "ftpc: Send error!\n");
		CLOSE(sockfd);
		exit(1);
	}

	// Show stats
	current = time(NULL);
	printf("ftpc: Sent %.02f KB (average %.02f KB/s)...\n", fileSize/1000.0,
	       (fileSize/1000.0)/difftime(current, start));

	fclose(file);
	printf("ftpc: Done\n");
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		printf("Usage: %s <host> <port> <file>\n", argv[0]);
		return 1;
	}
	printf("ftpc: Starting client...\n");

	fileName = basename(argv[3]);
	filePath = argv[3];
	openFile();
	printf("ftpc: Sending file %s of size %u bytes.\n", fileName, fileSize);

	openConnection(argv[1], argv[2]);
	sendFile();

	// Close connection
	sleep(5); //make sure everything is done
	CLOSE(sockfd);

	return 0;
}
