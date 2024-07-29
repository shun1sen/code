#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>//网络信息API
#include<stdio.h>
#include<unistd.h>
#include<assert.h>

int main(int argc,char *argv[])
{
    //assert(argc==2);
    char *host=argv[1];
    /*获取主机地址信息*/
    struct hostent* hostinfo=gethostbyname(host);
    assert(hostinfo);
    /*获取服务信息*/
    struct servent* serveinfo=getservbyname("daytime","tcp");
    assert(serveinfo);
    printf("daytime port is %d\n",ntohs(serveinfo->s_port));

    struct sockaddr_in address;
    address.sin_family=AF_INET;
    address.sin_port=serveinfo->s_port;
    address.sin_addr=*(struct in_addr*)*hostinfo->h_addr_list;

    int sockfd=socket(AF_INET,SOCK_STREAM,0);
    int result=connect(sockfd,(struct sockaddr*)&address,sizeof(address));
    assert(result!=-1);

    char buffer[128];
    result=read(sockfd,buffer,sizeof(buffer));
    assert(result>0);
    buffer[result]='\0';
    printf("the day time is:%s",buffer);
    close(sockfd);
    return 0;
}
