#include "rtp.h"
#include "util.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h> 


int main(int argc, char **argv) {
    if (argc != 6) {
        LOG_FATAL("Usage: ./sender [receiver ip] [receiver port] [file path] "
                  "[window size] [mode]\n");
    }
    srand(time(NULL));

    // your code here
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        LOG_FATAL("socket");
        exit(1);
    }
    

    struct timeval tval;
    tval.tv_sec=0;
    tval.tv_usec=100;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tval,sizeof(struct timeval));
    // 服务器地址和端口
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);  // 服务器IP地址

    socklen_t addr_len = sizeof(server_addr);

    // 文件路径
    char file_path[100] = {0};
    strcpy(file_path, argv[3]);
    // 发送端滑动窗口大小
    int window_size = atoi(argv[4]);

    // 使用哪种算法
    // mode=0就是回退N算法，mode=1就是选择重传算法
    int mode = atoi(argv[5]);

    rtp_packet_t syn_packet;
    syn_packet.rtp.flags = RTP_SYN;
    
    syn_packet.rtp.seq_num = rand();
    syn_packet.rtp.length = 0;
    syn_packet.rtp.checksum = 0;
 
    uint32_t cpt_checksum = compute_checksum(&syn_packet, sizeof(rtp_header_t));
    syn_packet.rtp.checksum = cpt_checksum;
    LOG_DEBUG("Sender's SYN_packet checksum is %d.\n",syn_packet.rtp.checksum);

    
    uint32_t ack_seq_num;
    uint32_t checksum = 0 ;
    rtp_packet_t tmp_packet;


    // 建立连接中

    int count_1 = 0;
    while (count_1 < 50){
        
        // 传递报文SYN
        int send_value = sendto(sockfd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (send_value == -1){
            LOG_FATAL("sendto error\n");
        }
        else{
            LOG_DEBUG("sendto success\n");
        }
        
        int recv_val = recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&server_addr, &addr_len);
        if (recv_val == -1){
            LOG_FATAL("recv failure\n");

        }
        
        else{
            if (tmp_packet.rtp.flags ==  RTP_SYN + RTP_ACK ){ // 说明receiver收到了来自sender的文件头
                LOG_DEBUG("SYNACK flag.\n");
                uint32_t ori_checksum = tmp_packet.rtp.checksum;
                tmp_packet.rtp.checksum = 0;
                checksum = compute_checksum(&tmp_packet, sizeof(rtp_header_t));
                LOG_DEBUG("ori_checksum is %d\n",ori_checksum);
                if (checksum != ori_checksum){
                    LOG_FATAL("wrong checksum,the computed checksum is %d\n",checksum);
                }
                else{
                    ack_seq_num = tmp_packet.rtp.seq_num;
                    LOG_DEBUG("receiver has received SYN from sender.\n");
                    break;//跳出循环
                }
                
            }
            else{
                if (tmp_packet.rtp.flags ==  RTP_SYN) LOG_FATAL("wrong flag! expect to receive synack, not syn\n");
                if (tmp_packet.rtp.flags ==  RTP_ACK) LOG_FATAL("wrong flag! expect to receive synack, not ack\n");
                if (tmp_packet.rtp.flags ==  RTP_FIN) LOG_FATAL("wrong flag! expect to receive synack, not fin\n");
                
                LOG_FATAL("other wrong flag!\n");
            }

        }
    }
    if (count_1==50){
        close(sockfd);
        LOG_FATAL("counter_1>50, failed to send SYN to receiver.\n");
    }
    int count_2 = 0;
    rtp_packet_t ack_packet;
    // 生成报文ACK
    // generate_ACK_header(&ACK_header,ack_seq_num);
    ack_packet.rtp.flags = RTP_ACK;
    ack_packet.rtp.seq_num = ack_seq_num;
    ack_packet.rtp.length = 0;
    ack_packet.rtp.checksum = 0;

    ack_packet.rtp.checksum = compute_checksum(&ack_packet, sizeof(rtp_header_t));
    LOG_DEBUG("generate ACK packet.\n");
    LOG_DEBUG("ackseqnum is %d\n",ack_seq_num);
    while (count_2 < 50){
        // 传递报文ACK
        sendto(sockfd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        LOG_DEBUG("send ACK to receiver.\n");
        // sleep(2);//等待2秒
        if (recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&server_addr, &addr_len)==-1){
            // 如果没有再收到ACKSYN，说明receiver已经收到了文件
            LOG_DEBUG("receiver has received ACK from sender.\n");
            LOG_DEBUG("successfully connected.\n");
            break;
        }
        count_2++;
    }
    if (count_2==50){
        close(sockfd);
        LOG_FATAL("counter_2>50, failed to send ACK to receiver.\n");
    }


    LOG_DEBUG("read file first.\n");
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        LOG_FATAL("Error opening file\n");
    }
    LOG_DEBUG("successfully open this file\n");
    rtp_packet_t* array = (rtp_packet_t*)malloc(capacity*sizeof(rtp_packet_t));

    int count = 0;
    // 先初始化容量capacity之内的
    while (count < capacity){
        
        array[count].rtp.seq_num = ack_seq_num + count;
        array[count].rtp.flags = 0;
        array[count].rtp.checksum = 0;
        // LOG_DEBUG("No.%d seq_num is %d\n",count,tmp_header.seq_num);
        
        int bytesRead = fread(array[count].payload, 1, PAYLOAD_MAX, file);

        if (bytesRead == PAYLOAD_MAX) {
            // 读取成功，处理数据
            // tmp_header.length = bytesRead;
            array[count].rtp.length = bytesRead;
            
            array[count].rtp.checksum = compute_checksum(&array[count], array[count].rtp.length+sizeof(rtp_header_t));

            LOG_DEBUG("length is %d\n",array[count].rtp.length);
            LOG_DEBUG("successfully read data\n");
            // printf("payload is: %s\n",array[count].payload);
            count=count+1;
            

        } else if (feof(file)) {
            // 到达文件末尾

            array[count].rtp.length = bytesRead;
            LOG_DEBUG("reach the end of file\n");
            array[count].rtp.checksum = compute_checksum(&array[count], sizeof(rtp_header_t)+array[count].rtp.length);
            // memcpy(&array[count].rtp,&tmp_header,sizeof(tmp_header));
            LOG_DEBUG("successfully read data\n");
            LOG_DEBUG("length is %d\n",array[count].rtp.length);
            // printf("payload is: %s\n",array[count].payload);
            count=count+1;
            fclose(file);
            break;

        } else if (ferror(file)) {
            // 读取过程中发生错误
            LOG_FATAL("fread\n");
        } else {
            LOG_FATAL("other cases\n");
            // 其他情况
        }
    }
    // 建立连接和文件读取都已完成，可以传输数据
    if (mode == 0){// 使用回退N算法
        LOG_DEBUG("use GBN to send data.\n");
        int base = 0; // 滑动窗口左值

        int recv_count = 0;
        while(base < count){
            int send_num = 0;
            
            while(send_num + base <= min(base + window_size - 1,count-1)){
                // count是目前包的总量
                int send_return = sendto(sockfd, &array[send_num+base], sizeof(rtp_packet_t), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                LOG_DEBUG("packet %d info: seq_num:%d, length:%d, flags:%d, checksum:%d\n",send_num+base,array[send_num+base].rtp.seq_num,array[send_num+base].rtp.length,array[send_num+base].rtp.flags,array[send_num+base].rtp.checksum);

                if (send_return == -1){
                    LOG_DEBUG("fail to send packet %d, send it again later.\n",send_num+base);
                    // 传不出去再传一遍
                }
                else{
                    LOG_DEBUG("successfully send packet %d\n",send_num+base);
                    send_num = send_num + 1;
                    
                }
            }
            recv_count = 0;
            while (recv_count<window_size){
                if (recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&server_addr, &addr_len)==-1){
                    LOG_DEBUG("have not receive any packet from receiver yet\n");
                    
                }
                else{
                    LOG_DEBUG("recv a packet from receiver.\n");
                    LOG_DEBUG("packet info: flags: %d, length: %d, checksum:%d, seq_num:%d\n",tmp_packet.rtp.flags,tmp_packet.rtp.length,tmp_packet.rtp.checksum,tmp_packet.rtp.seq_num);
                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                    tmp_packet.rtp.checksum = 0;
                    if (tmp_packet.rtp.flags!=RTP_ACK){
                        LOG_DEBUG("wrong flags\n");
                    }
                    else{
                        LOG_DEBUG("receive a ACK packet\n");
                        uint32_t cpt_checksum = compute_checksum(&tmp_packet,sizeof(rtp_header_t));
                        if (cpt_checksum!=ori_checksum){
                            LOG_DEBUG("wrong checksum\n");
                        }
                        else{
                            LOG_DEBUG("right checksum\n");
                            base = tmp_packet.rtp.seq_num-ack_seq_num;
                            LOG_DEBUG("base:%d\n",base);
                        }
                    }
            
                }
                recv_count=recv_count+1;
            }    
        }
        if (base == count){
            rtp_packet_t fin_packet;
            fin_packet.rtp.seq_num = ack_seq_num + count;
            fin_packet.rtp.length = 0;
            fin_packet.rtp.flags = RTP_FIN;
            fin_packet.rtp.checksum = 0;
            fin_packet.rtp.checksum = compute_checksum(&fin_packet, sizeof(rtp_header_t));
            int fin_count = 0;
            while (fin_count < 50){
                int send_v = sendto(sockfd, &fin_packet, sizeof(rtp_packet_t), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                if (send_v == -1){
                    LOG_DEBUG("fail to send fin_packet\n");
                    
                }
                else{
                    LOG_DEBUG("successfully send fin_packet\n");
                }
                int recv_vvvv = recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&server_addr, &addr_len);
                if (recv_vvvv == -1){
                    LOG_DEBUG("don't recv finack\n");

                }
                else{

                    LOG_DEBUG("recv a packet\n");
                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                    tmp_packet.rtp.checksum = 0;
                    uint32_t cpt_checksum = compute_checksum(&tmp_packet, sizeof(rtp_header_t));

                    if (tmp_packet.rtp.flags == RTP_ACK + RTP_FIN){
                        LOG_DEBUG("right flags\n");
                        if(tmp_packet.rtp.seq_num == fin_packet.rtp.seq_num){
                            LOG_DEBUG("right seq_sum\n");
                            if (ori_checksum == cpt_checksum){
                                LOG_DEBUG("right checksum\n");
                                LOG_DEBUG("recv ackfin packet\n");
                                LOG_DEBUG("close socket\n");
                                close(sockfd);
                                break;
                            }

                        }
                        else{
                            LOG_DEBUG("wrong seq_sum\n");
                        }
                    }
                    else{
                        LOG_DEBUG("wrong flags\n");
                    }
                }
                fin_count=fin_count+1;
            }

        }
        close(sockfd);
        free(array);
        LOG_DEBUG("Sender: exiting...\n");
    } 


    else{
        // 使用选择重传算法

        LOG_DEBUG("use OPT to send data.\n");
        int* check_list = (int*)malloc(capacity*sizeof(int));
        // if (check_list != NULL) {
        //     LOG_DEBUG("successfully create a check_list\n");
        //     memset(check_list, 0, capacity * sizeof(int));
        // }
        // else{
        //     LOG_FATAL("cannot alloc memory for check_list\n");
        // }










        int base = 0; // 滑动窗口左值
        // int right = min(window_size - 1,count);
        int right = window_size - 1;
        
        // count是包的总数量
        // right是滑动窗口右值
        int recv_count = 0;
        while(base < count){
            int send_num = 0;
            // 发送的时候只传没确认过的
            while(send_num <= min(window_size - 1,count)){
                // count是目前包的总量
                if (!check_list[send_num+base]){
                    LOG_DEBUG("packet %d havenot been checked yet, try to send\n",send_num + base);
                    int send_return = sendto(sockfd, &array[send_num+base], sizeof(rtp_packet_t), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
                    LOG_DEBUG("packet %d info: seq_num:%d, length:%d, flags:%d, checksum:%d\n",send_num + base,array[send_num+base].rtp.seq_num,array[send_num+base].rtp.length,array[send_num+base].rtp.flags,array[send_num+base].rtp.checksum);
                    // usleep(100000);// 模拟传输延迟
                    if (send_return == -1){
                        LOG_DEBUG("fail to send\n");
                        // 传不出去再传一遍
                    }
                    else{
                        LOG_DEBUG("successfully send packet %d\n",send_num + base);
                        
                        
                    }
                }
                else{
                    LOG_DEBUG("packet %d already have been checked\n",send_num + base);
                }
                send_num = send_num + 1;
                
            }
            recv_count = 0;
            int not_recv_count = 0;
            while (recv_count<window_size){
                if (recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&server_addr, &addr_len)==-1){
                    LOG_DEBUG("have not receive any packet from receiver yet\n");
                    not_recv_count = not_recv_count + 1;
                    break;
                }
                else{
                    LOG_DEBUG("recv a packet from receiver.\n");
                    LOG_DEBUG("flags: %d\n",tmp_packet.rtp.flags);
                    LOG_DEBUG("length: %d\n",tmp_packet.rtp.length);
                    LOG_DEBUG("checksum:%d\n",tmp_packet.rtp.checksum);
                    LOG_DEBUG("seq_num:%d\n",tmp_packet.rtp.seq_num);
                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                    tmp_packet.rtp.checksum = 0;
                    if (tmp_packet.rtp.flags!=RTP_ACK){
                        LOG_DEBUG("wrong flags\n");
                    }
                    else{
                        LOG_DEBUG("receive a ACK packet\n");
                        uint32_t cpt_checksum = compute_checksum(&tmp_packet,sizeof(rtp_header_t));
                        if (cpt_checksum!=ori_checksum){
                            LOG_DEBUG("wrong checksum\n");
                        }
                        else{
                            LOG_DEBUG("right checksum\n");
                            if (tmp_packet.rtp.seq_num < right + ack_seq_num + window_size  && tmp_packet.rtp.seq_num > base + ack_seq_num - window_size){
                            // 这个应该要改成seq_sum在范围内
                            // if (tmp_packet.rtp.seq_num == ack_seq_num + base+1 ){
                                LOG_DEBUG("probably seq_num\n");
                                check_list[tmp_packet.rtp.seq_num-ack_seq_num] = 1;
                                LOG_DEBUG("set packet %d checked.\n",tmp_packet.rtp.seq_num-ack_seq_num);
                                // 设置check_list
                                // int tmp_seq_num = tmp_packet.rtp.seq_num - ack_seq_num;
                                while(check_list[base]==1){
                                    LOG_DEBUG("base+1\n");
                                    base = base +1;
                                    LOG_DEBUG("base value:%d\n",base);
                                    right = right +1;
                                    LOG_DEBUG("right value:%d\n",right);

                                }
                                 
                            }
                            else{
                                LOG_DEBUG("seq_num is not in range, send again\n");
                                // int expt = ack_seq_num+base+1;
                                // LOG_DEBUG("expected seq_num is %d,but you send %d\n",expt,tmp_packet.rtp.seq_num);
                            }
                        }
                    }
            
                }
                recv_count=recv_count+1;
            }    
        }
        if (base == count){
            rtp_packet_t fin_packet;
            fin_packet.rtp.seq_num = ack_seq_num + count;
            fin_packet.rtp.length = 0;
            fin_packet.rtp.flags = RTP_FIN;
            fin_packet.rtp.checksum = 0;
            fin_packet.rtp.checksum = compute_checksum(&fin_packet, sizeof(rtp_header_t));
            int fin_count = 0;
            while (fin_count < 50){
                int send_v = sendto(sockfd, &fin_packet, sizeof(rtp_packet_t), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

                if (send_v == -1){
                    LOG_DEBUG("fail to send fin_packet\n");
                    
                }
                else{
                    LOG_DEBUG("successfully send fin_packet\n");
                }
                int recv_vvvv = recvfrom(sockfd, &tmp_packet, sizeof(tmp_packet), 0, (struct sockaddr*)&server_addr, &addr_len);
                if (recv_vvvv == -1){
                    LOG_DEBUG("don't recv finack\n");

                }
                else{

                    LOG_DEBUG("recv a packet\n");
                    uint32_t ori_checksum = tmp_packet.rtp.checksum;
                    tmp_packet.rtp.checksum = 0;
                    uint32_t cpt_checksum = compute_checksum(&tmp_packet, sizeof(rtp_header_t));

                    if (tmp_packet.rtp.flags == RTP_ACK + RTP_FIN){
                        LOG_DEBUG("right flags\n");
                        if(tmp_packet.rtp.seq_num == fin_packet.rtp.seq_num){
                            LOG_DEBUG("right seq_sum\n");
                            if (ori_checksum == cpt_checksum){
                                LOG_DEBUG("right checksum\n");
                                LOG_DEBUG("recv ackfin packet\n");
                                LOG_DEBUG("close socket\n");
                                close(sockfd);
                                break;
                            }

                        }
                        else{
                            LOG_DEBUG("wrong seq_sum\n");
                        }
                    }
                    else{
                        LOG_DEBUG("wrong flags\n");
                    }
                }
                fin_count=fin_count+1;
            }

        }
        close(sockfd);
        LOG_DEBUG("Sender: exiting...\n");
    } 
        
    return 0;
}


