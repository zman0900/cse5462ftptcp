#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

#define BUF_SIZE 1000

int sockfd;
FILE *file;
char *fileName;
int fileNameLen;
uint32_t fileSize;

void openConnection(char *host, char *port) {
	struct addrinfo hints, *servinfo, *p;
	int status;
	char addrString[INET6_ADDRSTRLEN];

	// Setup structures
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if ((status = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		exit(1);
	}

	// Connect to first result
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}
		// Try connect
		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			perror("connect");
			close(sockfd);
			continue;
		}
		// Socket connected
		break;
	}

	// Make sure socket is connected
	if (p == NULL)  {
		fprintf(stderr, "Failed to connect.\n");
		exit(1);
	}

	getInAddrString(p->ai_family, (struct sockaddr *)p->ai_addr, 
			addrString, sizeof addrString);
	printf("Connected to %s\n", addrString);

	// Clean up
	freeaddrinfo(servinfo);
}

void openFile() {
	struct stat st;

	if ((fileNameLen = strlen(fileName)) > FNAME_LEN - 1) {
		fprintf(stderr, "File name must be less than %d characters!\n", 
		        FNAME_LEN);
		exit(1);
	}
	if ((file = fopen(fileName, "rb")) == NULL) {
		perror("Couldn't open file");
		exit(1);
	}
	// Get size
	if (fstat(fileno(file), &st) == -1) {
		perror("Can't determine size of file");
		exit(1);
	}
	// Make sure size fits in 32bit
	if (st.st_size > UINT32_MAX) {
		fprintf(stderr, "File too large!\n");
		exit(1);
	}
	fileSize = st.st_size;
}

void sendFile() {
	char sendBuf[BUF_SIZE];
	uint32_t netSize = htonl(fileSize);
	int len;
	uint32_t remaining;

	// Send size
	memcpy(sendBuf, &netSize, 4);
	len = 4;
	if (sendAll(sockfd, sendBuf, &len) != 0) exit(1);

	// Send name in 20 bytes
	memset(sendBuf, '\0', FNAME_LEN);  // Not really necessary...
	memcpy(sendBuf, fileName, fileNameLen + 1);
	len = FNAME_LEN;
	if (sendAll(sockfd, sendBuf, &len) != 0) exit(1);

	// Read and send file
	remaining = fileSize;
	while (remaining >= BUF_SIZE) {
		if (fread(sendBuf, 1, BUF_SIZE, file) != BUF_SIZE) {
			fprintf(stderr, "File read error!\n");
			exit(1);
		}
		len = BUF_SIZE;
		if (sendAll(sockfd, sendBuf, &len) != 0) exit(1);
		remaining -= BUF_SIZE;
		printf("Sent %d bytes...\n", len);
	}
	// Send any remaining less buffer size
	if (fread(sendBuf, 1, remaining, file) != remaining) {
		fprintf(stderr, "File read error!\n");
	}
	len = remaining;
	if (sendAll(sockfd, sendBuf, &len) != 0) exit(1);
	printf("Sent %d bytes...\n", len);

	fclose(file);
	printf("Done\n");
}

int main(int argc, char *argv[]) {
	printf("Starting client...\n");
	if (argc != 4) {
		printf("Usage: %s <host> <port> <file>\n", argv[0]);
		return 1;
	}

	fileName = argv[3];
	openFile();
	printf("Sending file %s of size %u bytes.\n", fileName, fileSize);

	openConnection(argv[1], argv[2]);
	sendFile();

	return 0;
}
