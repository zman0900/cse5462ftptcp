#include <stdio.h>

int main(int argc, char *argv[]) {
	if (argc != 4) {
		printf("Usage: %s <troll-host> <troll-port> <listen-port>\n", argv[0]);
		return 1;
	}
	printf("Starting tcpd...\n");

	return 0;
}
