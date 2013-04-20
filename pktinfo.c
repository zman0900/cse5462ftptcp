#include <stdlib.h>

#include "pktinfo.h"

PktInfo *start = NULL;
PktInfo *end = NULL;
int num = 0;
int len = 0;

PktInfo* _pktinfo_find(uint32_t seqnum) {
	PktInfo *cur = start;
	while (cur != NULL && cur->seqnum != seqnum) {
		cur = cur->next;
	}
	return cur;
}

void pktinfo_add(uint32_t seqnum, int length) {
	PktInfo *insert = malloc(sizeof(PktInfo));
	insert->seqnum = seqnum;
	insert->length = length;
	insert->next = NULL;
	insert->prev = NULL;
	if (start == NULL) {
		start = insert;
		end = insert;
	} else {
		// Just stick it at the end
		end->next = insert;
		insert->prev = end;
		end = insert;
	}
	++num;
	len += length;
}

int pktinfo_get(uint32_t seqnum) {
	PktInfo *cur = _pktinfo_find(seqnum);
	if (cur == NULL) return -1;
	return cur->length;
}

int pktinfo_remove(uint32_t seqnum) {
	PktInfo *cur = _pktinfo_find(seqnum);
	if (cur == NULL) return -1;
	int ret = cur->length;
	// Remove from list
	if (cur->next != NULL) cur->next->prev = cur->prev;
	if (cur->prev != NULL) cur->prev->next = cur->next;
	if (cur == start) start = cur->next;
	if (cur == end) end = cur->prev;
	free(cur);
	--num;
	len -= ret;
	return ret;
}

int pktinfo_number() {
	return num;
}

int pktinfo_length() {
	return len;
}
