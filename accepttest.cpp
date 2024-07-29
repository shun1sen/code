#include<sys/socket.h>
#include<assert.h>
#include<netinet/in.h>//包含常用的网络和协议族的函数、常量
#include<arpa/inet.h>//网络字节序和主机字节序转换
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>

int main(int argc,char *argv[])
{
    if(argc<=2)
    {
        printf("usage:%s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char* ip=argv[1];
    int port = atoi(argv[2]);

    //创建socket地址
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);

    //创建socket
    int sock=socket(PF_INET,SOCK_STREAM,0);//TCP/IP协议族
    assert(sock>=0);

    //命名socket
    int ret=bind(sock,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    //监听
    ret=listen(sock,5);
    assert(ret!=-1);
    //暂停20s等待客户端连接和相关操作
    sleep(20);

    struct sockaddr_in client;
    socklen_t client_addrlength =sizeof(client);
    int connfd=accept(sock,(struct sockaddr*)&client,&client_addrlength);
    if(connfd<0)
    {
        printf("errno is : %d\n ,erron");
    }
    else
    {
        //接收打印处客户端的IP和端口号
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip:%s and port %d\n",inet_ntop(AF_INET,&client.sin_addr,remote,INET_ADDRSTRLEN),ntohs(client.sin_port));
        close(connfd);
    }

    close(sock);
    return 0;


}

