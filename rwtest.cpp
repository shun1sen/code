#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<errno.h>
#include<cstring>
#include<sys/stat.h>
#include<sys/types.h>
#include<fcntl.h>//提供文件描述符的函数

#define BUFFER_SIZE 1024
/*定义http状态码和状态信息*/
static const char*status_line[2]={"200,OK","500 Internet server error"};
int main(int argc,char*argv[])
{
    if(argc<=3)
    {
        printf("usage: %s ip_address port_number filename\n",basename(argv[0]));
        return 1;
    }
    const char* ip=argv[1];
    int port=atoi(argv[2]);
    const char*file_name=argv[3];
    
    struct sockaddr_in address;
    bzero(&address,sizeof(address));
    address.sin_family=AF_INET;
    inet_pton(AF_INET,ip,&address.sin_addr);
    address.sin_port=htons(port);
    
    int sock=socket(PF_INET,SOCK_STREAM,0);
    assert(sock>=0);
    
    int ret=bind(sock,(struct sockaddr*)&address,sizeof(address));
    assert(ret!=-1);

    ret=listen(sock,5);
    assert(ret!=-1);

    struct sockaddr_in client;
    socklen_t client_addrlength=sizeof(client);
    int connfd=accept(sock,(struct sockaddr*)&client,&client_addrlength);
    if(connfd<0)
    {
        printf("errno is: %d\n",errno);
    }
    else
    {
        /*保存HTTP应答的状态行，头部字段和一个空行的缓冲区*/
        char head_buf[BUFFER_SIZE];
        memset(head_buf,'\0',BUFFER_SIZE);
        /*用于存放目标文内容的应用缓存*/
        char *file_buf;
        /*获取文件的属性*/
        struct stat file_stat;
        /*是否是有效文件*/
        bool valid=true;
        /*记录缓存区使用的空间*/
        int len=0;
        if((stat(file_name,&file_stat)<0))/*文件不存在*/
        {
            valid=false;
        }
        else
        {
            if(S_ISDIR(file_stat.st_mode))/*判断文件是一个目录*/
                valid=false;
            else if(file_stat.st_mode&S_IROTH)/*当前用户读取文件的权限*/
            {
                int fd=open(file_name,O_RDONLY);
                file_buf=new char [file_stat.st_size+1];//分配内存为文件的大小加1
                memset(file_buf,'\0',file_stat.st_size+1);
                if(read(fd,file_buf,file_stat.st_size)<0)
                {
                    valid=false;
                }
            }
            else
            {
                valid=false;
            }
        }
        /*如果目标文件有效，则发送正常的HTTP应当*/
        if(valid)
        {
            /*下面代码将http应答的状态行，“CONNTCT=LENGTH"头部字段和一个空行一次加入head_buf中*/
            ret=snprintf(head_buf,BUFFER_SIZE-1,"%s %s\r\n","HTTP/1.1",status_line[0]);
            len+=ret;
            ret=snprintf(head_buf+len,BUFFER_SIZE-1-len,"Connect-Length: %ld\r\n",file_stat.st_size);
            len+=ret;
            ret=snprintf(head_buf+len,BUFFER_SIZE-1-len,"%s","\r\n");//\r\n换行，移动到下一行行首

            /*用writev将head_buf和file_buf写出*/
            struct iovec iv[2];
            iv[0].iov_base=head_buf;
            iv[0].iov_len=strlen(head_buf);
            iv[1].iov_base=file_buf;
            iv[1].iov_len=file_stat.st_size;
            ret=writev(connfd,iv,2);
        }
        /*文件无效，发送到客户端通知发送错误*/
        else
        {
            ret=snprintf(head_buf,BUFFER_SIZE-1,"%s %s\r\n","HTTP/1.1",status_line[0]);
            len+=ret;
            ret=snprintf(head_buf+len,BUFFER_SIZE-1-len,"%s","\r\n");//\r\n换行，移动到下一行行首
            send(connfd,head_buf,strlen(head_buf),0);
        }
        close(connfd);
        delete [] file_buf;
    }
    close(sock);
    return 0; 
}
