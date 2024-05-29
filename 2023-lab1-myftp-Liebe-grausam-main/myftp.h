#ifndef MYFTP_H

#include <string.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <iostream>

#define HTONL_12_CONSTANT htonl(12)
#define MAGIC_NUMBER_LENGTH 6
#define HEADER_LENGTH 12

#define MAX_FILE_NAME_LENGTH 256
#define SHA_LENGTH 256


struct myftp_header
{
    char m_protocol[MAGIC_NUMBER_LENGTH];   
    uint8_t m_type;                            
    uint8_t m_status;                          
    uint32_t m_length;                  
} __attribute__ ((packed));

// 报文常量
myftp_header OPEN_CONN_REQUEST = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA1, 2, htonl(12)};
myftp_header OPEN_CONN_REPLY = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA2, 1, htonl(12)};
myftp_header LIST_REQUEST = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA3, 2, htonl(12)};
myftp_header LIST_REPLY = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA4, 2, htonl(12)};//payload
myftp_header GET_REQUEST = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA5, 2, htonl(12)};//payload
myftp_header GET_REPLY = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA6, 0, htonl(12)};//默认是0，收到改成1
myftp_header FILE_DATA = {{'\xc1','\xa1','\x10','f','t','p'}, 0xff, 2, htonl(12)};//payload
myftp_header PUT_REQUEST = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA7, 2, htonl(12)};//PAYLOAD
myftp_header PUT_REPLY = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA8, 2, htonl(12)};
myftp_header SHA_REQUEST = {{'\xc1','\xa1','\x10','f','t','p'}, 0xA9, 2, htonl(12)};//PAYLOAD
myftp_header SHA_REPLY = {{'\xc1','\xa1','\x10','f','t','p'}, 0xAA, 0, htonl(12)};//STATUS
myftp_header QUIT_REQUEST = {{'\xc1','\xa1','\x10','f','t','p'}, 0xAB, 2, htonl(12)};
myftp_header QUIT_REPLY = {{'\xc1','\xa1','\x10','f','t','p'}, 0xAC, 2, htonl(12)};


void set_header(struct myftp_header * header,uint8_t type,uint8_t status,uint32_t length)
{  
    memcpy(header->m_protocol,"\xc1\xa1\x10""ftp",6);
    header->m_type = type;
    header->m_status = status;
    header->m_length = htonl(length);   //报文中的length以大端序存储
}


void dec_header(struct myftp_header * header,uint8_t *type,uint8_t *status,uint32_t *length)
{
    if(type)
        *type = header->m_type;
    if(status)
        *status = header->m_status;
    if(length)
        *length = ntohl(header->m_length);
}



int min(int a,int b)
{
    return a <= b ? a : b;
}
#endif