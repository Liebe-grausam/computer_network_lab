#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include "myftp.h"
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
using namespace std;

int get_string(char * buf)
{
    char c = getchar();
    while(c == ' ' || c == '\n'){continue;}
    if(c == EOF) return 0;
    int i = 0;
    while(c != ' ' && c != '\n')
    {
        buf[i++] = c;
        c = getchar();
    }
    buf[i] = '\0';
    
    return i;
}


int main(int argc, char ** argv) {
    char buf[32] = {0};
    int server;
    string str="";
    while(get_string(buf))
    {
        
        if(memcmp(buf,"open",4) == 0)
        {
            
            struct sockaddr_in addr;
            server = socket(AF_INET,SOCK_STREAM,0);
            addr.sin_family = AF_INET;
            memset(buf,0,sizeof(buf));
            get_string(buf);
            inet_pton(AF_INET,buf,&addr.sin_addr);
            memset(buf,0,sizeof(buf));
            get_string(buf);
            addr.sin_port = htons(atoi(buf));
            
            if(connect(server,(struct sockaddr*)&addr, sizeof(addr)) < 0)
            {
                printf("Connection failed.\n");
            }
            else
            {
                printf("Successfully connected!\n");
                struct myftp_header header;
                send(server,&OPEN_CONN_REQUEST,HEADER_LENGTH,0);
                while(recv(server,&header,HEADER_LENGTH,0))
                {
                    if(header.m_type == 0xA2){
                        printf("Successfully received an open_reply from server.\n");
                        break;
                    }
                        
                }
                
                if(header.m_status == 1)
                {
                    printf("The connection is established successfully.\n");
                }
                          
            }
            continue;
        }
   
        if (memcmp(buf,"get",3)==0){
            get_string(buf);
            struct myftp_header header;
            unsigned char temp[2048];
            memcpy(&header,&GET_REQUEST,HEADER_LENGTH);
            header.m_length = htonl(strlen(buf)+1+HEADER_LENGTH);
            send(server,&header,HEADER_LENGTH,0);
            send(server,buf,strlen(buf)+1,0);
            
            recv(server,&header,HEADER_LENGTH,0);
            
            if (header.m_status){
                
                FILE *fp = fopen(buf,"wb");
                recv(server,&header,HEADER_LENGTH,0);
                
                int length = header.m_length-HEADER_LENGTH;
                recv(server,temp,2048,0);
                fwrite(temp,1,2048,fp);
                fclose(fp);

            }


        }
     
        if (memcmp(buf,"put",3) == 0){
            string filename;
            cin>>filename;
            
            // send(server,filename.c_str(),filename.length()+1,0);
            if(access(filename.c_str(),F_OK) == 0)
            { 
                struct myftp_header header;

                memcpy(&header,&PUT_REQUEST,HEADER_LENGTH);
                header.m_length = htonl(HEADER_LENGTH+filename.length()+1);
                send(server,&header,HEADER_LENGTH,0);
                send(server,filename.c_str(),filename.length()+1,0);

                recv(server,&header,HEADER_LENGTH,0);

                struct stat statbuf;
                stat(filename.c_str(),&statbuf);
                memcpy(&header,&FILE_DATA,HEADER_LENGTH);
                header.m_length=htonl(HEADER_LENGTH+statbuf.st_size);

                send(server,&header,HEADER_LENGTH,0);
                // file_data文件头

                char temp[1024];
                FILE *fp = fopen(filename.c_str(),"rb");
                
                fread(temp,sizeof(char),1024,fp);
                   
                send(server,temp,1024,0);
                
                fclose(fp);
            }
            


        }
   
    }
    return 0;
}