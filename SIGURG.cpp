#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<signal.h>
#include<unistd.h>
#include<errno.h>
#include<cstring>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<pthread.h>

#define BUFFER_SIZE 1024
static int connfd;



/*信号处理函数*/
void sig_handler(int sig)
{
    /*保留原来的errno，在函数最后恢复，以保证函数的可重入性*/
    int save_errno=errno;
    char buffer[BUFFER_SIZE];
    memset(buffer,'\0',BUFFER_SIZE);
    int ret=recv(connfd,buffer,BUFFER_SIZE-1,MSG_OOB);
    printf("got %d bytes of oob data '%s'\n",ret,buffer);
    errno=save_errno;
}
/*设置信号处理函数*/
void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=sig_handler;
    sa.sa_flags|=SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
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
    address.sin_port=htons(port);

    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);

    int ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret=listen(listenfd,5);
    assert(ret!=-1);

    struct sockaddr_in client;
    socklen_t length=sizeof(client);
    connfd=accept(listenfd,(struct sockaddr*)&client,&length);
    if(connfd<0)
    {
        printf("errno is: %d \n",errno);
    }
    else
    {
        addsig(SIGURG);
        fcntl(connfd,F_SETOWN,getpid());
        char buffer[BUFFER_SIZE];
        while(1)
        {
            memset(buffer,'\0',BUFFER_SIZE);
            ret=recv(connfd,buffer,BUFFER_SIZE-1,0);
            if(ret<=0)
            {
                break;
            }
            printf("got %d bytes of normal data : %s \n",ret,buffer);
        }
        close(connfd);
    }
    close(listenfd);
    return 0;

}
