#ifndef __RTP_H
#define __RTP_H

#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h> 
#include "util.h"




#ifdef __cplusplus
extern "C" {
#endif

#define PAYLOAD_MAX 1461
#define capacity 1000000

// flags in the rtp header
typedef enum RtpHeaderFlag {
    RTP_SYN = 0b0001,
    RTP_ACK = 0b0010,
    RTP_FIN = 0b0100,
} rtp_header_flag_t;
// 0b 表示二进制数

typedef struct __attribute__((__packed__)) RtpHeader {
    uint32_t seq_num;  // Sequence number
    uint16_t length;   // Length of data; 0 for SYN, ACK, and FIN packets
    uint32_t checksum; // 32-bit CRC
    uint8_t flags;     // See at `RtpHeaderFlag`
} rtp_header_t;

typedef struct __attribute__((__packed__)) RtpPacket {
    rtp_header_t rtp;          // header
    char payload[PAYLOAD_MAX]; // data
} rtp_packet_t;


int min (int a, int b) {
    if (a<b)
        return a;
    else
        return b;
}
    




#ifdef __cplusplus
}
#endif

#endif // __RTP_H
