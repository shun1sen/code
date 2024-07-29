#include<fcntl.h>
#include<sys/epoll.h>
#include<stdio.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<arpa/inet.h>//网络字节序和主机字节序转换
#include<netinet/in.h>//协议族
#include<assert.h>
#include<errno.h>
#include<cstring>
#include<unistd.h>
#include<sys/types.h>/*提高基本数据类型，如size_t*/
#include<pthread.h>

#define  MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_SETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd,int fd)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}
int main(int argc,char*argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char*ip=argv[1];
    int port=atoi(argv[2]);

    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    

}
