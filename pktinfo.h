#ifndef __PKTINFO_H_INCLUDED__
#define __PKTINFO_H_INCLUDED__

#include <stdint.h>

// Use to keep track of unack'd packets for sender
// or out of order packets for receiver
// Doubly linked list probably isn't the best choice, but it will do
typedef struct PktInfo PktInfo;
struct PktInfo {
	uint32_t seqnum;
	int length;
	PktInfo *next;
	PktInfo *prev;
};

void pktinfo_add(uint32_t seqnum, int length);
// Returns length if found, -1 if not found, leaves in list
int pktinfo_get(uint32_t seqnum);
// Returns length if found, -1 if not found, removes from list
int pktinfo_remove(uint32_t seqnum);
int pktinfo_size();

#endif
