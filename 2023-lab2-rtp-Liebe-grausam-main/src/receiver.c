#include "rtp.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h> 


int main(int argc, char **argv) {
    if (argc != 5) {
        LOG_FATAL("Usage: ./receiver [listen port] [file path] [window size] "
                  "[mode]\n");
    }
    

    // your code here
    // 创建UDP套接字
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    

    // 绑定服务器地址和端口
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    char file_path[100] = {0};
    strcpy(file_path, argv[2]);
    int window_size = atoi(argv[3]);
    int mode = atoi(argv[4]);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    printf("Server is waiting for incoming messages...\n");

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    struct timeval tval;
    tval.tv_sec = 5;
    tval.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tval,sizeof(struct timeval));

    rtp_packet_t tmp_packet;
    uint32_t synack_seqnum = 0;
    while(1){
        int recv_vall = recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr *)&client_addr, &addr_len);
        if (recv_vall==-1){
            LOG_FATAL("Haven't received anything.\n");
            close(sockfd);
            return 0;
        }// 没收到就关闭连接
        else{
            LOG_DEBUG("length:%d\n",tmp_packet.rtp.length);
            LOG_DEBUG("flags:%d\n",tmp_packet.rtp.flags);
            LOG_DEBUG("seq_num:%d\n",tmp_packet.rtp.seq_num);
            LOG_DEBUG("checksum:%d\n",tmp_packet.rtp.checksum);
            if (tmp_packet.rtp.flags != RTP_SYN){
                LOG_DEBUG("wrong flag.\n");
            }
            else{
                LOG_DEBUG("correct SYN flag!\n");
                uint32_t ori_checksum = tmp_packet.rtp.checksum;
                tmp_packet.rtp.checksum = 0;
                tmp_packet.rtp.length = 0;
                uint32_t cpt_checksum =  compute_checksum(&tmp_packet, sizeof(rtp_header_t));
                
                LOG_DEBUG("compute checksum is %d\n", cpt_checksum);
                if (cpt_checksum != ori_checksum){
                    LOG_DEBUG("wrong checksum! original checksum is %d\n",ori_checksum);
                }
                else{
                    LOG_DEBUG("Right SYN header!\n");
                    break;
                }
            }
        }

    }
    

    int x = tmp_packet.rtp.seq_num;
    synack_seqnum = x+1;
    
    rtp_packet_t synack_packet;
    synack_packet.rtp.flags = RTP_ACK + RTP_SYN;
    synack_packet.rtp.seq_num = x + 1;
    synack_packet.rtp.length = 0;
    synack_packet.rtp.checksum = 0;
    synack_packet.rtp.checksum = compute_checksum(&synack_packet, sizeof(rtp_header_t));
    
    struct timeval tv;
    tv.tv_sec=0;
    tv.tv_usec=100;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv,sizeof(struct timeval));

    int count = 0;
    while (count<50){
        sendto(sockfd, &synack_packet, sizeof(synack_packet), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
        if (recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&client_addr, &addr_len)==-1){
            // receiver没有收到ACK
            // 计数器+1，重传
            count++;
        }
        else{
            if (tmp_packet.rtp.flags != RTP_ACK){
                
                if (tmp_packet.rtp.flags == RTP_ACK + RTP_SYN){
                    LOG_DEBUG("Wrong flags! receiver want to receive ACK, not a SYNACK.\n");
                }
                if (tmp_packet.rtp.flags == RTP_SYN){
                    LOG_DEBUG("Wrong flags! receiver want to receive ACK, not a SYN.\n");
                }
                count++;

            }
            else{
                LOG_DEBUG("Received ACK flag.\n");
                if (tmp_packet.rtp.seq_num!=x+1){
                    LOG_DEBUG("wrong seq num\n");
                }
                else{
                    uint32_t cpt_checksum = 0;
                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                    tmp_packet.rtp.checksum = 0;
                    cpt_checksum = compute_checksum(&tmp_packet,sizeof(rtp_header_t));
                    if (ori_checksum != cpt_checksum){
                        LOG_DEBUG("wrong checksum\n");
                    }
                    else{
                        LOG_DEBUG("Successfully connected!\n");
                        break;

                    }
                }
            }
            
            
        }

    }
    if (count == 50){
        LOG_FATAL("Failed to connected.\n");
        close(sockfd);
        return 0;
    }

    if (mode == 0){
        FILE *fp = fopen(file_path,"wb");
        rtp_packet_t previous_ack_packet;
        rtp_packet_t ack_packet;
        int base = 0;
        int right = window_size;
        
        while(1){
        
            char recv_buf[1500];
            rtp_packet_t tmp_packet;
            
            if (recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&client_addr, &addr_len)!=-1){
                LOG_DEBUG("receive a packet.\n");
                memcpy(&tmp_packet, recv_buf, sizeof(recv_buf));
                LOG_DEBUG("seq_num:%d\n",tmp_packet.rtp.seq_num);
                LOG_DEBUG("length:%d\n",tmp_packet.rtp.length);
                LOG_DEBUG("checksum:%d\n",tmp_packet.rtp.checksum);
                LOG_DEBUG("flags:%d\n",tmp_packet.rtp.flags);

                if(tmp_packet.rtp.length!=0){
                    if (tmp_packet.rtp.flags!=0){
                        LOG_DEBUG("packet context error, length>0 but flags is not data flag! throw it.\n");
                    }
                    else{

                        LOG_DEBUG("data flag!\n");
                        if (tmp_packet.rtp.length>PAYLOAD_MAX){

                            LOG_DEBUG("packet context error, too large length! throw it.\n");
                        }
                        else{
                            // 暂时觉得应该没问题
                            // 后续检验
                            uint32_t ori_checksum = tmp_packet.rtp.checksum;
                            tmp_packet.rtp.checksum = 0;
                            uint32_t cpt_checksum = compute_checksum(&tmp_packet,tmp_packet.rtp.length+sizeof(rtp_header_t));
                
                            if (tmp_packet.rtp.seq_num != base + synack_seqnum){
                                int next_seq_num = base + synack_seqnum;
                                LOG_DEBUG("wrong seq_num, expected seq_num is %d\n",next_seq_num);
                                LOG_DEBUG("seq_num is %d\n",tmp_packet.rtp.seq_num);
                                LOG_DEBUG("send previous ack packet again\n");
                                int send_vv = sendto(sockfd, &previous_ack_packet, sizeof(previous_ack_packet), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                                if (send_vv<0){
                                    LOG_DEBUG("send error\n");
                                }
                                

                                
                            }
                            else{
                                LOG_DEBUG("right seq_sum\n");
                                if (ori_checksum != cpt_checksum){
                                    LOG_DEBUG("wrong checksum, cpt_checksum is %d, but ori_checksum is %d\n",cpt_checksum,ori_checksum);
                                }
                                else{
                                    LOG_DEBUG("right checksum\n");
                                    LOG_DEBUG("ready to write to file.\n");
                                    
                                    if (fp == NULL){
                                        LOG_FATAL("unable to open file path\n");
                                    }
                                    LOG_DEBUG("successfully open the file\n");
                                    
                                    int fwrite_value = fwrite(tmp_packet.payload,1,tmp_packet.rtp.length,fp);
                                    if (fwrite_value < tmp_packet.rtp.length){
                                        LOG_DEBUG("unable to write all words to file.txt\n");
                                    }
                                    else{
                                        LOG_DEBUG("Successfully write all of packet %d to file.txt\n",base);
                                        
                                        ack_packet.rtp.length = 0;
                                        ack_packet.rtp.flags = RTP_ACK;
                                        ack_packet.rtp.seq_num = tmp_packet.rtp.seq_num + 1;
                                        ack_packet.rtp.checksum = 0;
                                        ack_packet.rtp.checksum = compute_checksum(&ack_packet,sizeof(rtp_header_t));
                                        int send_vv = sendto(sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                                        if (send_vv<0){
                                            LOG_DEBUG("send error\n");
                                        }
                                        
                                    }                               
                                    base = base + 1;
                                    LOG_DEBUG("base:%d\n",base);
                                    right = right +1;
                                    LOG_DEBUG("right:%d\n",right);
                                }
                            }

                
                        }

                    }
                }
                else{
                    // length等于0的八成是一个finpacket
                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                    tmp_packet.rtp.checksum = 0;
                    uint32_t cpt_checksum = compute_checksum(&tmp_packet,tmp_packet.rtp.length+sizeof(rtp_header_t));
                
                    if (tmp_packet.rtp.flags!=0){
                    
                        if (tmp_packet.rtp.flags == RTP_FIN){
                            LOG_DEBUG("recv fin packet, ready to end socket\n");
                            
                            if (ori_checksum != cpt_checksum){
                                LOG_DEBUG("wrong checksum, cpt_checksum is %d, but ori_checksum is %d\n",cpt_checksum,ori_checksum);
                            }
                            else{
                                LOG_DEBUG("right checksum\n");
                                rtp_packet_t finack_packet;
                                finack_packet.rtp.seq_num = tmp_packet.rtp.seq_num;
                                finack_packet.rtp.length = 0;
                                finack_packet.rtp.flags = RTP_FIN + RTP_ACK;
                                finack_packet.rtp.checksum = compute_checksum(&finack_packet,sizeof(rtp_header_t));
                                if (sendto(sockfd, &finack_packet, sizeof(finack_packet), 0, (struct sockaddr *)&client_addr, sizeof(client_addr))!=-1){
                                    LOG_DEBUG("Successfully send finack_packet\n");
                                    LOG_DEBUG("finack_packet info: seq_num:%d, length:%d, flags:%d, checksum:%d\n",finack_packet.rtp.seq_num,finack_packet.rtp.length,finack_packet.rtp.flags,finack_packet.rtp.checksum);
                                }
                                else{
                                    LOG_DEBUG("fail to send\n");
                                }
                                // close(sockfd);
                            
                                int recv_vv = recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&client_addr, &addr_len);
                                if (recv_vv<0){
                                    close(sockfd);
                                }

                            }
                            break;

                        }
                        else{
                            LOG_DEBUG("unknown flag!\n");
                        }
                    }
                    else{
                        LOG_DEBUG("wrong packet! data flag but length is 0.\n");
                    }

                }
                
            }     
        }
        close(sockfd);

        LOG_DEBUG("Receiver: exiting...\n");
    }
    else{
        // 选择重传部分
        FILE *fp = fopen(file_path,"wb");
        char (*file_buf)[PAYLOAD_MAX]=(char(*)[PAYLOAD_MAX])malloc(sizeof(char)*capacity*PAYLOAD_MAX); 
        int* file_length = (int*)malloc(capacity*sizeof(int));
        rtp_packet_t* ack_packet_array = (rtp_packet_t*)malloc(capacity*sizeof(rtp_packet_t));
        int* check_list = (int*)malloc(capacity*sizeof(int));
        int base = 0;
        int right = window_size-1;
        
        while(1){
        
            char recv_buf[1500];
            rtp_packet_t tmp_packet;
           
            if (recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr*)&client_addr, &addr_len)!=-1){
                LOG_DEBUG("receive a packet.\n");
                memcpy(&tmp_packet, recv_buf, sizeof(recv_buf));
                LOG_DEBUG("packet info: seq_num:%d,length:%d,checksum:%d,flags:%d\n",tmp_packet.rtp.seq_num,tmp_packet.rtp.length,tmp_packet.rtp.checksum,tmp_packet.rtp.flags);


                if(tmp_packet.rtp.length!=0){
                    if (tmp_packet.rtp.flags!=0){
                        LOG_DEBUG("packet context error, length>0 but flags is not data flag! throw it.\n");
                    }
                    else{
                        // 这是一个数据包
                        LOG_DEBUG("data flag!\n");
                        if (tmp_packet.rtp.length>PAYLOAD_MAX){
                            LOG_DEBUG("packet context error, too large length! throw it.\n");
                        }
                        else{
                            if (tmp_packet.rtp.seq_num < base-window_size+synack_seqnum|| tmp_packet.rtp.seq_num >= right +window_size+synack_seqnum){
                                LOG_DEBUG("index out of range\n");
                            }
                            else{
                                LOG_DEBUG("probable seq_sum\n");

                                if (check_list[tmp_packet.rtp.seq_num-synack_seqnum]){
                                    LOG_DEBUG("send its ack packet again\n");
                                    if (sendto(sockfd, &ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum], sizeof(ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum]), 0, (struct sockaddr *)&client_addr, sizeof(client_addr))<0){
                                        LOG_DEBUG("send error\n");
                                    }       
                                }
                                else{
                                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                                    tmp_packet.rtp.checksum = 0;
                                    uint32_t cpt_checksum = compute_checksum(&tmp_packet,tmp_packet.rtp.length+sizeof(rtp_header_t));
                        

                                    if (ori_checksum != cpt_checksum){
                                        LOG_DEBUG("wrong checksum, cpt_checksum is %d, but ori_checksum is %d\n",cpt_checksum,ori_checksum);
                                    }
                                    else{
                                        LOG_DEBUG("right checksum\n");
                                        // copy
                                        LOG_DEBUG("ready to send ACK\n");
                                        check_list[tmp_packet.rtp.seq_num-synack_seqnum]=1;
                                        ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum].rtp.length = 0;
                                        ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum].rtp.flags = RTP_ACK;
                                        ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum].rtp.seq_num = tmp_packet.rtp.seq_num;
                                        // 选择重传seqsum就是原来的
                                        ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum].rtp.checksum = 0;
                                        ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum].rtp.checksum = compute_checksum(&ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum],sizeof(rtp_header_t));
                                        int send_vv = sendto(sockfd, &ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum], sizeof(ack_packet_array[tmp_packet.rtp.seq_num-synack_seqnum]), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                                        if (send_vv<0){
                                            LOG_DEBUG("send error\n");
                                        }

                                        LOG_DEBUG("copy it to file_buf.\n");
                                        memcpy(&file_buf[tmp_packet.rtp.seq_num-synack_seqnum],&tmp_packet.payload,tmp_packet.rtp.length);
                                        file_length[tmp_packet.rtp.seq_num-synack_seqnum] = tmp_packet.rtp.length;

                                        while(check_list[base]==1){
                                            int fwrite_value = fwrite(file_buf[base],1,file_length[base],fp);
                                            if (fwrite_value < file_length[base]){
                                                LOG_DEBUG("unable to write all words to file.txt\n");
                                            }
                                            else{
                                                LOG_DEBUG("Successfully write all of packet %d to file.txt\n",base);
                                            }
                                            base = base + 1;
                                            right = right + 1;
                                            LOG_DEBUG("base:%d, right:%d\n",base,right);
                                        }
                                    }
                                }                              
                            }             
                        }
                    }
                }
                else{
                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                    tmp_packet.rtp.checksum = 0;
                    uint32_t cpt_checksum = compute_checksum(&tmp_packet,tmp_packet.rtp.length+sizeof(rtp_header_t));
                
                    if (tmp_packet.rtp.flags!=0){
                    
                        if (tmp_packet.rtp.flags == RTP_FIN){
                            LOG_DEBUG("recv fin packet, ready to end socket\n");
                            
                            if (ori_checksum != cpt_checksum){
                                LOG_DEBUG("wrong checksum, cpt_checksum is %d, but ori_checksum is %d\n",cpt_checksum,ori_checksum);
                            }
                            else{
                                LOG_DEBUG("right checksum\n");
                                rtp_packet_t finack_packet;
                                finack_packet.rtp.seq_num = tmp_packet.rtp.seq_num;
                                finack_packet.rtp.length = 0;
                                finack_packet.rtp.flags = RTP_FIN + RTP_ACK;
                                finack_packet.rtp.checksum = compute_checksum(&finack_packet,sizeof(rtp_header_t));
                                if (sendto(sockfd, &finack_packet, sizeof(finack_packet), 0, (struct sockaddr *)&client_addr, sizeof(client_addr))!=-1){
                                    LOG_DEBUG("Successfully send finack_packet\n");
                                    LOG_DEBUG("finack_packet info: seq_num:%d, length:%d, flags:%d, checksum:%d\n",finack_packet.rtp.seq_num,finack_packet.rtp.length,finack_packet.rtp.flags,finack_packet.rtp.checksum);
                                }
                                else{
                                    LOG_DEBUG("fail to send\n");
                                }
                                
                                int recv_vv = recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&client_addr, &addr_len);
                                if (recv_vv<0){
                                    close(sockfd);
                                }

                            }
                            break;

                        }
                        else{
                            LOG_DEBUG("unknown flag!\n");
                        }
                    }
                    else{
                        LOG_DEBUG("wrong packet! data flag but length is 0.\n");
                    }

                }
            }
        }
        free(file_buf);
        free(file_length);
        close(sockfd);

        LOG_DEBUG("Receiver: exiting...\n");
        free(check_list);
        free(ack_packet_array);
    }
    return 0;
}
