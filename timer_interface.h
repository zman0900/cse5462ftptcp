#ifndef __TIMER_INTERFACE_H_INCLUDED__
#define __TIMER_INTERFACE_H_INCLUDED__

#include <stdint.h>
#include <sys/time.h>

#define PACKET_SIZE 21

// The following return negative values on failure
int timer_start(const int socket, const struct timeval * const time,
                const uint32_t seqnum);
int timer_cancel(const int socket, const uint32_t seqnum);
// Queries timer process for seqnum of expired timer
// Only call if data is waiting on socket
uint32_t timer_getExpired(const int socket);

#endif
