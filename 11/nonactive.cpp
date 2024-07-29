#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <pthread.h>
#include "LST_time.h"

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd=0;

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
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

void sig_hander(int sig)
{
    int save_errno=errno;
    int msg=sig;
    send(pipefd[1],(char*)&msg,1,0);//处理信号
    errno=save_errno;
}   

void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=sig_hander;
    sa.sa_flags|=SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

void timer_handler()
{
    /*定时器处理任务，实际就是调用tick函数*/
    timer_lst.tick();
    /*因为一次alarm调用只会引起一次sigalrm信号，所以我们要重新定时，从而不断触发SIGALRM信号*/
    alarm(TIMESLOT);
}

/*定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之*/
void cb_func(client_data*user_data)
{
    epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    close(user_data->sockfd);
    printf("close fd %d\n",user_data->sockfd);
}

int main(int argc,char*argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number \n",basename(argv[0]));
        return 1;
    }
    const char*ip=argv[1];
    int port=atoi(argv[2]);
    
    struct sockaddr_in adderss;
    bzero(&adderss,sizeof(adderss));
    adderss.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&adderss.sin_addr);
    adderss.sin_port=htons(port);

    int listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listen>=0);

    int ret=bind(listenfd,(struct sockaddr*)&adderss,sizeof(adderss));
    assert(ret!=-1);

    ret=listen(listenfd,5);
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd,listenfd);

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
    assert(ret!=-1);
    setnonblocking(pipefd[1]);
    addfd(epollfd,pipefd[0]);

    /*设置信号处理函数*/
    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server=false;

    client_data*users=new client_data[FD_LIMIT];
    bool timeout=false;
    alarm(TIMESLOT);//定时

    while(!stop_server)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0)&&(errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)
        
        {
            int sockfd=events[i].data.fd;
            if(sockfd==listenfd)/*处理新的客户连接*/
            {
                struct sockaddr_in client_address;
                socklen_t lenghth=sizeof(client_address);
                int connfd=accept(listenfd,(struct sockaddr*)&client_address,&lenghth);
                addfd(epollfd,connfd);
                users[connfd].address=client_address;
                users[connfd].sockfd=connfd;
                /*创建定时器，设置其回调函数和超时事件，然后绑定定时器与用户数据，组后将会定时器添加到链表timer_lst中*/
                util_timer*timer=new util_timer;
                timer->user_data=&users[connfd];
                timer->cb_func=cb_func;
                time_t cur =time(NULL);
                timer->expire=cur+3*TIMESLOT;
                users[connfd].timer=timer;
                timer_lst.add(timer);
            }
            /*处理信号*/
            else if((sockfd==pipefd[0])&&(events[i].events&EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret=recv(pipefd[0],signals,1024,0);
                if(ret==-1)
                {
                    continue;
                }
                else if(ret==0)
                {
                    continue;
                }
                else
                {
                    for(int i=0;i<ret;i++)
                    {
                        switch(signals[i])
                        {
                            case SIGALRM:
                            /*用timeout变量标定由定时任务需要处理，但不立即处理定时任务。这是因为定时任务优先级不高，优先处理其他任务*/
                            {timeout=true;
                            break;}
                            case SIGTERM:
                            {
                                stop_server=true;
                            }

                        }
                        
                    }
                }
            }
            else if(events[i].events&EPOLLIN)
            {
                memset(users[sockfd].buf,'\0',BUFFER_SIZE);
                ret=recv(sockfd,users[sockfd].buf,BUFFER_SIZE-1,0);
                printf("get %d bytes of client_data %s from %d\n",ret,users[sockfd].buf,sockfd);
                util_timer*timer=users[sockfd].timer;
                if(ret<0)
                {
                    /*如果发生读取错误，则关闭连接，并移除对应的定时器*/
                    if(errno!=EAGAIN)
                    {
                        cb_func(&users[sockfd]);
                        if(timer)
                        {
                            timer_lst.del_timer(timer);
                        }
                    }
                }
                else if(ret==0)
                {
                    /*如果对方关闭连接，我么也需要关闭连接，并移除对应的定时器*/
                    cb_func(&users[sockfd]);
                    if(timer)
                    {
                        timer_lst.del_timer(timer);
                    }
                }
                else
                {
                    /*如果某个客户连接上由数据可读，则我们需要调整连接对应的定时器，以延迟该连接被关闭时间*/
                    if(timer)
                    {
                        time_t cur=time(NULL);
                        timer->expire=cur+5*TIMESLOT;
                        printf("adjust timer once\n");
                        timer_lst.adjust(timer);
                    }
                }
            }
            else
            {}
        }
        if(timeout)
        {
            timer_handler();
            timeout=false;
        }
    }
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete [] users;
    return 0;
}

