#ifndef __TCPHEADER_H_INCLUDED__
#define __TCPHEADER_H_INCLUDED__

#include <stdint.h>

#define TCP_HEADER_SIZE 32

// Tcp header with tcp timestamps option
#pragma pack(1)  // Not portable, but makes this union work, and this is easier
typedef union _header {
	struct _field {
		uint16_t sport;
		uint16_t dport;
		uint32_t seqnum;
		uint32_t acknum;
		uint8_t doffset_ns;
		uint8_t flags;
		uint16_t winsize;
		uint16_t checksum;
		uint16_t urgptr;
		uint8_t tsopt_kind;
		uint8_t tsopt_len;
		uint32_t tsval;
		uint32_t tsecr;
		uint16_t pad;
	} field;
	unsigned char packed[TCP_HEADER_SIZE];
} Header;

/*
 * Allocates and returns pointer to a tcp header with the specified properties
 */
Header* tcpheader_create(uint16_t sport, uint16_t dport, uint32_t seqnum,
                        uint32_t acknum, int isSyn, int isAck, int isFin,
                        uint32_t tsecr, void *data, int data_bytes,
                        void *packet);

/*
 * These return 0 for false, positive for true
 */
int tcpheader_isack(Header *h);
int tcpheader_isfin(Header *h);
int tcpheader_issyn(Header *h);

int tcpheader_verifycrc(void *packet, int bytes);

#endif
