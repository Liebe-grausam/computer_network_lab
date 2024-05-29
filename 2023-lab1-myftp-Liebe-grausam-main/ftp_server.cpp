#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "myftp.h"
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

// void bigtest_recv(int sock, int len,FILE *fp){
//     unsigned char* buffer = (unsigned char*)malloc(2048);
//     int ret = 0;
//     while (ret < len) {
//         memset(buffer, 0, 2048);
//         recv(sock, buffer, min(len - ret,2048), 0);
//         fwrite(buffer, 1, min(len - ret,2048), fp);
//         ret += 2048; 
//     }
//     free(buffer);
// }
// // 不会实现。


int main(int argc, char ** argv) {

    int server_socket, client;
    int flag = 0; //是否连接上的标志
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    // 创建服务器套接字
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    // 设置服务器地址信息
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET,argv[1],&server_addr.sin_addr);
    server_addr.sin_port = htons(atoi(argv[2]));

    // 将服务器套接字绑定到地址
    bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 开始监听连接请求
    listen(server_socket, 1);

    // 接受客户端连接请求
    
    while(1)
    {
        while (!flag){
            socklen_t addr_len = sizeof(client_addr);
            client=accept(server_socket,(struct sockaddr *)&client_addr,&addr_len);
            if(client==-1){
                std::cout << "connect error" << std::endl;
                
            }else{
                std::cout << "connect created" << std::endl;
                flag=1;
                break;
            }
        }
        struct myftp_header header;
        recv(client,&header,HEADER_LENGTH,0);
        unsigned char type;
        unsigned int length;
        dec_header(&header,&type,NULL,&length);


        //OPEN_CONN_REQUEST  

        if(type == 0xA1)
        {
            
            send(client,&OPEN_CONN_REPLY,HEADER_LENGTH,0);
            continue;
        }
        
        //LIST_REQUEST
        if(type == 0xA3)
        {
            FILE * fp;
            char ls_answer[2048] = {0};
            char buf[64] = {0};
            int length = 0;
            fp = popen("ls","r");
            length = fread(ls_answer,1,2048,fp);
            ls_answer[length] = 0;
            myftp_header list_reply_header = LIST_REPLY;
            list_reply_header.m_length = htonl(HEADER_LENGTH+length+1);
            // list_reply_header.m_length = HEADER_LENGTH+length+1;
            send(client,&list_reply_header,HEADER_LENGTH,0);
            send(client,ls_answer,length+1,0);
            continue;
        }


        //GET_REQUEST
        if(type == 0xA5)
        {
            char filename[16];
            recv(client,filename,length-HEADER_LENGTH,0);
            if(access(filename,F_OK) == 0)
            {
                memcpy(&header,&GET_REPLY,sizeof(GET_REPLY));
                header.m_status = 1;
                send(client,&header,HEADER_LENGTH,0);


                struct stat statbuf;
                stat(filename,&statbuf);
                set_header(&header,0xff,0,HEADER_LENGTH+statbuf.st_size);
                send(client,&header,HEADER_LENGTH,0);
                unsigned char temp[1024];
                FILE *fp = fopen(filename,"rb");
                int length = 0;
                while(1)
                {
                    length = fread(temp,sizeof(char),1024,fp);
                    if(length == 0) break;
                    send(client,temp,length,0);
                }
                fclose(fp);
            }
            else
            {
                send(client,&GET_REPLY,HEADER_LENGTH,0);
            }
        }

        // PUT_REQUEST
        if (type==0xA7){
            char filename[128];
            recv(client,filename,length-HEADER_LENGTH,0);
            send(client,&PUT_REPLY,HEADER_LENGTH,0);
            recv(client,&header,HEADER_LENGTH,0);
            // 收到filedata文件
            unsigned int length = (int)(header.m_length-HEADER_LENGTH);
            char * temp = (char*)malloc(length);
            recv(client,temp,length,0);
            FILE *fp=fopen(filename, "wb");
            int res = 0;
            while (res<length){
                fwrite(temp+res, 1, 2048, fp);
                res+=length;
            }
            fclose(fp);
            free(temp);
        }
        


        //SHA_REQUEST
        if (type == 0xA9)
        {
            char filename[16]; 
            recv(client, filename, length - HEADER_LENGTH, 0);

            struct myftp_header sha_reply_header;
            unsigned char status = 0; // 默认文件不存在

            if (access(filename, F_OK) == 0)
            {
                memcpy(&sha_reply_header,&SHA_REPLY,sizeof(SHA_REPLY));
                sha_reply_header.m_status = 1;

                send(client, &sha_reply_header, HEADER_LENGTH, 0);

                // 使用 popen 执行 sha256sum 命令并获取结果
                char command[512];
                snprintf(command, sizeof(command), "sha256sum %s", filename);
                FILE *fp = popen(command, "r");
                if (fp)
                {
                    char sha_value[SHA_LENGTH + 1]; // 保存 SHA256 校验值，包括结尾的 NULL 字符
                    memset(sha_value, 0, sizeof(sha_value));
                    int sha_num = fread(sha_value, 1, SHA_LENGTH, fp);
                    pclose(fp);

                    // 发送 SHA256 校验值
                    size_t sha_length = strlen(sha_value);

                    set_header(&sha_reply_header, 0xff, status, HEADER_LENGTH + sha_length + 1);
                    send(client, &sha_reply_header, HEADER_LENGTH, 0);
                    send(client, sha_value, sha_length + 1, 0);
                }
            }
            else
            {
                send(client, &SHA_REPLY, HEADER_LENGTH, 0);
            }
        }
            
             
        //QUIT_REQUEST

        if(type == 0xAB)
        {
            send(client,&QUIT_REPLY,HEADER_LENGTH,0);
            close(client);
            break;
        }
    
    }



}



