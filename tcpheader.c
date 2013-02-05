#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>

#include "common.h"
#include "tcpheader.h"

Header* tcpheader_create(uint16_t sport, uint16_t dport, uint32_t seqnum,
                        uint32_t acknum, int isSyn, int isAck, int isFin,
                        uint32_t tsecr) {
	Header *h = malloc(TCP_HEADER_SIZE);

	h->field.sport = htons(sport);  // Tells other side where to send
	h->field.dport = htons(dport);
	h->field.seqnum = htonl(seqnum);
	h->field.acknum = htonl(acknum);
	h->field.doffset_ns = 0;  // No hton needed for single bytes
	h->field.doffset_ns |= (8 << 4);
	h->field.flags = 0;  // No hton needed for single bytes
	if (isAck) h->field.flags |= (1 << 4);
	if (isSyn) h->field.flags |= (1 << 1);
	if (isFin) h->field.flags |= 1;
	h->field.winsize = htons(WINSIZE * MSS);
	h->field.checksum = 0;
	h->field.urgptr = 0;
	h->field.tsopt_kind = 8;
	h->field.tsopt_len = 10;
	h->field.tsval = htonl(getTimestamp());
	h->field.tsecr = htonl(tsecr);
	h->field.pad = 0;

	return h;
}

int tcpheader_isack(Header *h) {
	return h->field.flags & (1 << 4);
}

int tcpheader_isfin(Header *h) {
	return h->field.flags & 1;
}

int tcpheader_issyn(Header *h) {
	return h->field.flags & (1 << 1);
}
