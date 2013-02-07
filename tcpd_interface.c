#include <sys/socket.h>
#include <sys/types.h>

#include "tcpd_interface.h"

int SOCKET(int domain, int type, int protocol) {
	return -666;
}

int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
	return -666;
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
