#define _GUN_SOURCE 1
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

#define USER_LIMIT 5/*最大用户数量*/
#define BUFFER_SIZE 64 /*读缓冲区大小*/
#define FD_LIMIT 65535 /*文件描述符数量限制*/

struct client_data
{
    sockaddr_in address;
    char*write_buf;
    char buf[BUFFER_SIZE];
};
int setnoblocking(int fd) /*设置非阻塞*/
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option=old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int main(int argc,char* argv[])
{
    if(argc<=2)
    {
        printf("usage:%s ip_address port_number\n",basename(argv[0]));
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

    /*创建nums数组，分配FD_LIMIT个client_data对象。可以预期：每个可能的socket连接都可以获得一个这样的对象，并且socket的值可以之直接用来索引socket连接对应的client—data对象，
    这是将socket和客户数据关联的简单而有效的方式*/
    client_data*users=new client_data[FD_LIMIT];
    /*尽管我们分配了足够多的client_data对象，但为了提高poll的性能，仍然有必要限制用户的数量*/
    pollfd fds[USER_LIMIT+1];
    int user_counter=0;
    for(int i=1;i<USER_LIMIT;i++)
    {
        fds[i].fd=-1;
        fds[i].events=0;
    }
    fds[0].fd=listenfd;
    fds[0].events=POLLIN | POLLERR;//告诉poll监听可读和错误事件类型
    fds[0].revents=0;

    while(1)
    {
        ret=poll(fds,user_counter+1,-1);
        if(ret<0)
        {
            printf("poll failure\n");
            break;
        }
        for(int i=0;i<user_counter+1;++i)
        {
            if((fds[i].fd==listenfd)&&(fds[i].revents&POLLIN))//添加连接的客户端
            {
                struct sockaddr_in client_address;//客户端地址数据内核自动分配
                socklen_t client_lenght=sizeof(client_address);
                int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_lenght);
                if(connfd<0)
                {
                    printf("errno is:%d",errno);
                    continue;
                }
                /*如果请求太多，则关闭新到的连接*/
                if(user_counter>=USER_LIMIT)
                {
                    const char*info="too many users\n";
                    printf("%s\n",info);
                    send(connfd,info,strlen(info),0);
                    close(connfd);
                    continue;
                }
                /*对于新的连接，同时修改fds和users数组，users[connfd]对应于新连接文件描述符connfd的客户数据*/
                user_counter++;
                users[connfd].address=client_address;
                
                setnoblocking(connfd);
                fds[user_counter].fd=connfd;
                fds[user_counter].events=POLLIN|POLLERR|POLLRDHUP;
                fds[user_counter].revents=0;
                printf("come a new user,now have %d users\n",user_counter);
            }
            else if(fds[i].revents&POLLERR)
            {
                printf("get an error from %d\n",fds[i].fd);
                char errors[100];
                memset(errors,'\0',100);
                socklen_t length=sizeof(errors);
                if(getsockopt(fds[i].fd,SOL_SOCKET,SO_ERROR,&errors,&length)<0)
                {
                    printf("get socket option failed\n");
                }
                continue;
            }
            else if(fds[i].revents&POLLRDHUP)
            {
                /*如果客户端关闭连接，则服务器也关闭对应的连接，用户总数减1 */
                users[fds[i].fd]=users[fds[user_counter].fd];//复制活跃连接到当前位置，以便后面的删除
                close(fds[i].fd);
                fds[i]=fds[user_counter];
                i--;
                user_counter--;
                printf("a client left\n");
            }
            else if(fds[i].revents&POLLIN)
            {
                int connfd=fds[i].fd;
                memset(users[connfd].buf,'\0',BUFFER_SIZE);
                ret=recv(connfd,users[connfd].buf,BUFFER_SIZE-1,0);
                printf("get %d bytes of client data %s from %d\n",ret,users[connfd].buf,connfd);
                if(ret<0)
                {
                    /*如果读操作出错，则关闭连接*/
                    if(errno!=EAGAIN)
                    {
                        close(connfd);
                        users[fds[i].fd]=users[fds[user_counter].fd];
                        fds[i]=fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if(ret==0)
                {

                }
                else{
                    /*如果接收到客户数据，则通知其他socket连接准备写数据*/
                    for(int j=1;j<=user_counter;++j)
                    {
                        if(fds[j].fd==connfd)
                        {
                            continue;
                        }
                        fds[j].events|=~POLLIN;
                        fds[j].events|=POLLOUT;
                        users[fds[j].fd].write_buf=users[connfd].buf;
                    }

                }
            }
            else if(fds[i].revents&POLLOUT)
            {
                int connfd=fds[i].fd;
                if(!users[connfd].write_buf)
                {
                    continue;
                }
                ret=send(connfd,users[connfd].write_buf,strlen(users[connfd].write_buf),0);
                users[connfd].write_buf=NULL;
                /*写完后需要重新注册fds[i]上的可读事件*/
                fds[i].events|=~POLLOUT;
                fds[i].events|=POLLIN;
            }
        }
    }
    delete [] users;
    close(listenfd);
    return 0;
}