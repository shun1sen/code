#include<sys/socket.h>
#include<fcntl.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<assert.h>
#include<cstring>

static const int CONTROL_LEN=CMSG_LEN(sizeof(int));

void send_fd(int fd,int fd_to_send)
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];

    iov[0].iov_base=buf;
    iov[0].iov_len=1;
    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;

    cmsghdr cm;
    cm.cmsg_len=CONTROL_LEN;
    cm.cmsg_level=SOL_SOCKET;
    cm.cmsg_type=SCM_RIGHTS;
    *(int*)CMSG_DATA(&cm)=fd_to_send;/* 将文件描述符赋值到控制消息的数据部分 */
    msg.msg_control=&cm;
    msg.msg_controllen=CONTROL_LEN;

    sendmsg(fd,&msg,0);
}

int recv_fd(int fd)
{
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];
    iov[0].iov_base=buf;
    iov[0].iov_len=1;
    msg.msg_name=NULL;
    msg.msg_namelen=0;
    msg.msg_iov=iov;
    msg.msg_iovlen=1;

    cmsghdr cm;
    msg.msg_control=&cm;
    msg.msg_controllen=CONTROL_LEN;

    recvmsg(fd,&msg,0);

    int fd_to_read=*(int*)CMSG_DATA(&cm);
    return fd_to_read;
}

int main()
{
    int pipefd[2];
    int fd_to_pass=0;
    /*创建父子进程间的管道，文件描述符pipefd[0]和pipefd[1]都是UNIX域socket*/
    int ret=socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
    assert(ret!=-1);
    pid_t pid=fork();
    assert(pid>=0);
    if(pid==0)
    {
        close(pipefd[0]);
        fd_to_pass=open("test.txt",O_RDWR,0666);
        /*子进程通过管道将文件描述符发送给父进程，如果打开失败，则子进程将标准输入文件描述符发送到父进程*/
        send_fd(pipefd[1],(fd_to_pass>0)?fd_to_pass:0);
        close(fd_to_pass);
        exit(0);
    }

    close(pipefd[1]);
    fd_to_pass=recv_fd(pipefd[0]);
    char buf[1024];
    memset(buf,'\0',1024);
    read(fd_to_pass,buf,1024);
    printf("I got fd %d and data %s\n",fd_to_pass,buf);
    close(fd_to_pass);
}
