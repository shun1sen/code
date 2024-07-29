#include<sys/types.h>
#include<sys/socket.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<unistd.h>
#include<assert.h>
#include<string.h>
#include<stdio.h>
#include<signal.h>

static bool stop=false;
static void handle_term(int sig)
{
    stop=true;
}
int main(int argc,char *argv[])
{
    signal(SIGTERM,handle_term);
    if(argc<=3)
    {
        printf("usage: %s ip_address port_number backlog\n",basename(argv[0]));
        return 1;
    }
    const char*ip =argv[1];
    int port=atoi(argv[2]);
    int backlog=atoi(argv[3]);
    
    int sock=socket(PF_INET,SOCK_STREAM,0);// 创建socket（协议族 ，数据方式）
    assert(sock>=0); //条件返回错误程序终止

    //创建socket地址
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;//地址族
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);

    int ret=bind(sock,(struct sockaddr*)&address,sizeof(address));//命名socket，将socket和地址绑定
    assert(ret!=-1);//=-1命名失败，结束程序

    ret=listen(sock,backlog);//监听
    assert(ret!=-1);

    while(!stop)
    {
        sleep(1);
    }

    close(sock);
    return 0;
}