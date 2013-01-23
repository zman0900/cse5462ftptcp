#ifndef __COMMON_H_INCLUDED__
#define __COMMON_H_INCLUDED__

#include <netinet/in.h>

void getInAddrString(int af, struct sockaddr *sa, char *dst, socklen_t size);

#endif
