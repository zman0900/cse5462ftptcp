#include <arpa/inet.h>

#include "common.h"

void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size) {
	const void *src;
	if (sa->sa_family == AF_INET) {
		src = &(((struct sockaddr_in*)sa)->sin_addr);
	} else {
		src = &(((struct sockaddr_in6*)sa)->sin6_addr);
	}
	inet_ntop(af, src, dst, size);
}
