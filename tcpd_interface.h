#ifndef __TCPD_INTERFACE_H_INCLUDED__
#define __TCPD_INTERFACE_H_INCLUDED__

#include <sys/socket.h>
#include <sys/types.h>

int SOCKET(int domain, int type, int protocol);
int BIND(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int ACCEPT(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int CONNECT(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t SEND(int sockfd, const void *buf, size_t len, int flags);
ssize_t RECV(int sockfd, void *buf, size_t len, int flags);
int CLOSE(int fd);

#endif
