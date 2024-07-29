
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
#include<sys/epoll.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/mman.h>
#include<sys/stat.h>

/*最大用户数量*/
/*读缓冲区大小*/
/*文件描述符数量限制*/
/*最大事件数量*/
/*进程限制*/
enum constant{USER_LIMIT=5,BUFFER_SIZE=64,FD_LIMIT=65535,MAX_EVENT_NUMBER=1024,PROCESS_LIMIT=65536};

/*处理一个客户连接必要的数据*/
struct client_data
{
    sockaddr_in address;
    int connfd;
    pid_t pid;/*处理这个连接的子进程的PID*/
    int pipefd[2]; /*和父进程通信的管道*/
};
static const char*shm_name="/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char* share_mem=0;
int ret;
/*客户连接数组，进程用客户连接的编号来索引这个数组，即可取得相关的客户连接数据*/
client_data*users=0;/*0相当于NULL*/
/*子进程和客户连接的额映射关系表，用进程的PID来索引这个数组，即可取得该进程所处理的客户连接的编号*/
int *sub_process=0;
/*当前客户的数量*/
int user_count=0;
bool stop_child = false;

int setnonblocking(int fd)
{
    int old_option=fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd=fd;
    event.events=EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void sig_handler(int sig)
{
    int save_errno=errno;
    int msg=sig;
    send(sig_pipefd[1],&msg,1,0);
    errno=save_errno;
}

void addsig(int sig,void(*handler)(int),bool restate=true)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler=handler;
    if(restate)
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL)!=-1);
}

void del_resourse()
{
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}

/*停止一个进程*/
void child_term_handler(int sig)
{
    stop_child=true;
}

/*子进程运行函数。参数idx指出该子进程处理的客户端连接的编号，users是保存所有客户连接数据的数组，参数share_mem指出共享内存的起始地址*/
int run_child(int idx,client_data*users,char*share_mem)
{
    epoll_event events[MAX_EVENT_NUMBER];
    /*子进程使用I/O复用技术来同时监听两个文件描述符，客户连接socket、父进程通信的管道文件描述符*/
    int child_epollfd=epoll_create(5);
    assert(child_epollfd!=-1);
    int connfd = users[idx].connfd;
    addfd(child_epollfd,connfd);
    int pipefd=users[idx].pipefd[1];
    addfd(child_epollfd,pipefd);

    /*子进程需要设置自己的信号处理函数*/
    addsig(SIGTERM,child_term_handler,true);
    while(!stop_child)
    {
        int number=epoll_wait(child_epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0)&&(errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0;i<number;i++)
        {
            int sockfd=events[i].data.fd;
            /*本子进程负责客户连接有数据到达*/
            if((sockfd==connfd)&&(events[i].events&EPOLLIN))
            {
                memset(share_mem+idx*BUFFER_SIZE,'\0',BUFFER_SIZE);
                /*将客户数据读取到对应的读缓存中，该读缓存是共享内存的一段，它开始于idx*BUFFERSIZE处，长度为BUFFER_SIZE字节。因此么个客户连接的读缓存时共享的*/
                ret=recv(connfd,share_mem+idx*BUFFER_SIZE,BUFFER_SIZE-1,0);
                if(ret<0)
                {
                    if(errno!=EAGAIN)
                        stop_child=true;
                }
                else if(ret==0)
                {
                    stop_child=true;
                }
                else
                {
                    /*成功读取客户数据后就通知主进程（通过管道）处理*/
                    send(pipefd,(char*)&idx,sizeof(idx),0);
                }
            }
            /*主进程通知本进程将第client个客户的数据发生到本进程负责的客户端*/
            else if((sockfd==pipefd)&&(events[i].events&EPOLLIN))
            {
                int client=0;
                /*接收主进程发送来的数据，即由客户数据达到的连接的编号*/
                ret=recv(sockfd,(char*)&client,sizeof(client),0);
                if(ret<0)
                {
                    if(errno!=EAGAIN)
                        stop_child=true;
                }
                else if(ret==0)
                {
                    stop_child=true;
                }
                else
                {
                    send(connfd,share_mem+client*BUFFER_SIZE,BUFFER_SIZE,0);
                }
            }
            else
            {
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0; 
}

int main(int argc,char*argv[])
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number \n ",basename(argv[0]));
    }

    const char*ip=argv[1];
    int port=atoi(argv[2]);
    
    
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);

    listenfd=socket(PF_INET,SOCK_STREAM,0);
    assert(listenfd>=0);

    ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret=listen(listenfd,5);
    assert(ret!=-1);

    user_count=0;
    users=new client_data[USER_LIMIT+1];
    sub_process=new int [PROCESS_LIMIT];
    for(int i=0; i< PROCESS_LIMIT;i++)
    {
        sub_process[i]=-1;
    }

    epoll_event events[MAX_EVENT_NUMBER];
    epollfd=epoll_create(5);
    assert(epollfd!=-1);
    addfd(epollfd,listenfd);

    ret=socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);//本地协议族
    assert(ret!=-1);
    setnonblocking(sig_pipefd[1]);
    addfd(epollfd,sig_pipefd[0]);

    addsig(SIGCHLD,sig_handler);
    addsig(SIGTERM,sig_handler);
    addsig(SIGINT,sig_handler);
    addsig(SIGPIPE,sig_handler);
    bool stop_server=false;
    bool terminate=false;

    /*创建共享内存，作为所有客户socket连接的读缓存*/
    shmfd=shm_open(shm_name,O_CREAT | O_RDWR,0666);
    assert(shmfd!=-1);
    ret=ftruncate(shmfd,USER_LIMIT*BUFFER_SIZE);//ftruncate是一个POSIX标准的系统调用，用于截断或扩展一个已经打开的文件的大小。这个函数可以用于减少或增加文件的大小，但通常用于将文件的大小减少到指定的长度。
    assert(ret!=-1);

    share_mem=(char*)mmap(NULL,USER_LIMIT*BUFFER_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,shmfd,0);//申请内存空间
    assert(share_mem!=MAP_FAILED);
    close(shmfd);

    while(!stop_server)
    {
        int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((number<0)&&(errno!=EINTR))
        {
            printf("epoll failure\n");
            break;
        }
        for(int i=0; i<number; i++)
        {
            int sockfd=events[i].data.fd;
            /*监听状态，连接客户端*/
            if(sockfd==listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t cli_add_length=sizeof(client_address);
                int connfd=accept(sockfd,(struct sockaddr*)&client_address,&cli_add_length);
                if(connfd<0)
                {
                    printf("errno is :%d\n",errno);
                    continue;//继续遍历文件描述符和事件
                }
                if(user_count>USER_LIMIT)
                {
                    const char*info="too many users\n";
                    printf("%s",info);
                    send(connfd,info,strlen(info),0);
                    close(connfd);
                    continue;
                }
                /*保存第usercount个用户的数据*/
                users[user_count].address=client_address;
                users[user_count].connfd=connfd;
                /*在子进程和主进程间建立管道，以传递必要的数据*/
                ret=socketpair(PF_UNIX,SOCK_STREAM,0,users[user_count].pipefd);
                assert(ret!=-1);
                pid_t pid;
                if(pid<0)
                {
                    close(connfd);
                    continue;
                }
                else if(pid==0)//子进程
                {
                    close(connfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);//数据由子进程流向父进程
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count,users,share_mem);
                    munmap((void*)share_mem,USER_LIMIT*BUFFER_SIZE);
                    exit(0);
                }
                else
                {
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd,users[user_count].pipefd[0]);
                    users[user_count].pid=pid;
                    /*记录新的客户连接在数组users的索引值，建立进程pid和该索引值之间的映射关系*/
                    sub_process[pid]=user_count;
                    user_count++;
                }
            }
            /*处理信号事件*/
            else if((sockfd==sig_pipefd[0])&&(events[i].events& EPOLLIN))
            {
                int sig;
                char signals[1024];
                ret=recv(sig_pipefd[0],signals,sizeof(signals),0);
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
                    for(int i=0;i<ret;++i)
                    {
                        switch(signals[i])
                        {
                            /*子进程退出，表示由某个客户端关闭了连接*/
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while((pid=waitpid(-1,&stat,WNOHANG))>0)
                                {
                                    /*用子进程的pid取得被关闭的客户连接的编号*/
                                    int del_user=sub_process[pid];
                                    sub_process[pid]=-1;
                                    if((del_user<0)||(del_user>USER_LIMIT))
                                    {
                                        continue;
                                    }
                                    /*清除第del_user个客户连接的相关数据*/
                                    epoll_ctl(epollfd,EPOLL_CTL_DEL,users[del_user].pipefd[0],0);
                                    close(users[del_user].pipefd[0]);
                                    users[del_user]=users[--user_count];
                                    sub_process[users[del_user].pid]=del_user;

                                }
                                if(terminate && user_count==0)
                                {
                                    stop_server= true;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                /*结束服务器程序*/
                                printf("kill all the child now\n");
                                if(user_count==0)
                                {
                                    stop_server = true;
                                    break;
                                }
                                for(int i=0;i<user_count;i++)
                                {
                                    int pid=users[i].pid;
                                    kill(pid,SIGTERM);
                                }
                                terminate=true;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }

            /*某个子程序像父程序写入数据*/
            else if(events[i].events&EPOLLIN)
            {
                int child=0;
                /*读取管道数据，child变量记录了哪个客户连接由数据到达*/
                ret=recv(sockfd,(char*)&child,sizeof(child),0);
                printf("read data from child accross pipe\n");
                if(ret== -1)
                {
                    continue;
                }
                else if(ret==0)
                {
                    continue;
                }
                else
                {
                    /*向除了负责处理第child个客户连接的子进程之外的其他子进程发送消息，通知他们有客户写入数据*/
                    for(int j=0;j<user_count;j++)
                    {
                        if(users[j].pipefd[0]!=sockfd)
                        {
                            printf("send data to child accross pipe\n");
                            send(users[j].pipefd[0],(char*)&child,sizeof(child),0);
                        }
                    }
                }

            }
        }
    }

    del_resourse();
    return 0;
}

