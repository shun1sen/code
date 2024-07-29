#include<sys/types.h>/*提高基本数据类型，如size_t*/
#include<netinet/in.h>//包含常用的网络和协议族的函数、常量
#include<arpa/inet.h>//网络字节序和主机字节序转换
#include<fcntl.h>//提供文件描述符的函数
#include<sys/socket.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>//提供了一组与操作系统交互的函数，用于执行各种系统级操作,定义了一些 POSIX 标准的符号常量，如 STDIN_FILENO、STDOUT_FILENO 和 STDERR_FILENO，分别代表标准输入、输出和错误的文件描述符。
#include<errno.h>
#include<string.h>
#include<stdlib.h>//本程序中提高数据转换函数atoi()
#include<poll.h>
#include<time.h>

#define BUFFER_SIZE 1023
int setnoblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);//获得文件新的状态描述
    return old_option;//返回旧的标识符，以便日后恢复文件
}
/*超时连接函数，参数分别为服务器IP地址端口号和超时时间，函数成功时返回已经处理连接状态的socket，失败返回-*/
int unblock_connect(const char*ip,int port,int time)
{
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);

    int sockfd=socket(PF_INET,SOCK_STREAM,0);
    int fdopt=setnoblocking(sockfd);//旧的文件描述符
    int ret=connect(sockfd,(struct sockaddr*)&address,sizeof(address));
    if(ret==0)
    {
        /*如果连接成功，则恢复sockfd属性，并立即返回*/
        printf("connect with server immediately\n");
        fcntl(sockfd,F_SETFL,fdopt);
        return sockfd;
    }
    else if(errno!=EINPROGRESS)
    {
        /*如果没有立即建立，那么只有当errno时EINPROGRESS时才表示连接还在进行，否则返回错误*/
        printf("unblock connect not support\n");
        return -1;
    }

    fd_set readfds;
    fd_set writefds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(sockfd,&writefds);
    timeout.tv_sec=time;
    timeout.tv_usec=0;

    ret=select(sockfd+1,NULL,&writefds,NULL,&timeout);
    if(ret<=0)
    {
        /*select超时或出错，立即返回*/
        printf("connection time out\n");
        close(sockfd);
        return -1;
    }
    if(!FD_ISSET(sockfd,&writefds))
    {
        printf("no events on sockfd found\n");
        close(sockfd);
        return -1;
    }

    int error=0;
    socklen_t length=sizeof(error);
    /*调用getsockopt来获取并清楚sockfd上的错误*/
    if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&error,&length)<0);
    {
        printf("get socket option failed\n");
        close(sockfd);
        return -1;
    }
    /*错误不为0表示连接错误*/
    if(error!=0)
    {
        printf("connection failed after select with the error: %d \n",error);
        close(sockfd);
        return -1;
    }
    else{
        printf("connection ready after select with the socket:%d\n",sockfd);
        fcntl(sockfd,F_SETFL,fdopt);
        return sockfd;
    }
}
int main(int argc,char*argv[])
{
    if(argc<=2)
    {
        printf("usage : %s ip_address porta-number\n",basename(argv[0]));
        return 1;
    }
    const char*ip=argv[1];
    int port=atoi(argv[2]);
    int sockfd=unblock_connect(ip,port,10);
    if(sockfd<0)
    {
        return 1;
    }
    close(sockfd);
    return 0;
}
