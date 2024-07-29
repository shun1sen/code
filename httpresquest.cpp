#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<error.h>
#include<cstring>
#include<fcntl.h>
#include<errno.h>
#define BUFFER_SIZE 4096 //读缓冲区大小
/*主机状态的两种可能状态，分别表示：当前正在分析请求行，当前正在分析头部字段*/
enum CHECK_STATE{CHECK_STATE_REOUESTLINE=0,CHECK_STATE_HEADER};
/*从状态机的三种可能状态，行的读取状态，分别表示：读取到一个完整行、行出错和行数据尚且完整*/
enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};
/*http请求结果：NO_REOUEST表示请求不完整，需要继续读取客户数据；GET_REQUEST表示获得了一个完整的客户请求；BAD_REQUEST表示客户请求有语法错误；
FORBIDDEN_REQUEST表示客户对资源没有足够的访问权限；INTERNAL_ERRON表示服务器内部错误；CLOSED_CONNECTON表示客户端已经关闭连接*/
enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,FORBIDDEN_REQUEST,INTERNAL_ERRON,CLOSED_CONNECTON};
/*为了简化问题，我们没有给客户端发送一个完整的HTTP报文，而只是根据服务器的处理结果发送如下成功或失败信息*/
static const char*szret[]={"I get a correct result\n","Something wrong\n"};

/*从状态机，用于解析一行内容*/
LINE_STATUS parse_line(char*buffer,int& checked_index,int & read_index )
{
    char temp;
    /*checked_index指向buffer(应用程序的读缓冲区)中当前正在分析的字节，read_index指向buffer中客户数据的尾部的下一个字节。buffer中从0到checked——index字节都已经分析完毕，
    第checked_index到（read_index-1)字节由下面的循环挨个分析*/
    for(;checked_index<read_index;checked_index++)
    {
        temp=buffer[checked_index];//当前分字节
        /*如果当前字节是\r，即回车键，则说明可能读到下一个完整的行*/
        if(temp=='\r')
        {
            /*如果\r字符碰巧是当前buffer中最后一个已经被读入的客户端数据，那么这次分析没有读取到一个完整的行，返回LINE_OPEN*/
            if((checked_index+1)==read_index)
                return LINE_OPEN;
            /*如果下一个字符为'\n',换行符，说明我们读取到一个完整行*/
            else if(buffer[checked_index+1]=='\n')
            {
                buffer[checked_index++]='\0';
                buffer[checked_index++]='\0';
                return LINE_OK;
            }
            /*否则的话，说明客户端发送的HTTP请求有问题*/
            return LINE_BAD;
        }
        /*如果当前字节为'\n',也可能读取到完整行*/
        else if(temp=='\n')
        {
            if((checked_index>1)&&buffer[checked_index-1]=='\r')
            {
                buffer[checked_index-1]='\0';
                buffer[checked_index++]='\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    /*如果没有遇到'\r',说明还需要继续读取，返回LINE_OPEN*/
    return LINE_OPEN;   
}

/*分析请求行*/
HTTP_CODE parse_requsestline(char*temp,CHECK_STATE&checkstate)
{
    char*url =strpbrk(temp," \t");//使用 strpbrk 函数查找 temp 字符串中第一次出现空格或制表符的位置，并将该位置的指针赋值给 url。
    /*如果请求行中没有空白字符或者'\t'字符，则HTTP请求必有问题*/
    if(!url)
    {
        return BAD_REQUEST;
    }
    *url++='\0';//url位置的空白字符替换为字符串结束符'\0',然后url向前移动一位
    char*method=temp;
    if(strcasecmp(method,"GET")==0)/*GET方法*/
    {
        printf("the request method is GET\n");
    }
    else
    {
        return BAD_REQUEST;
    }

    url+=strspn(url," \t");
    char*version=strpbrk(url," /t");
    if(!version)
    {
        return BAD_REQUEST;
    }
    *version++='\0';
    /*仅支持http 1.1*/
    if(strcasecmp(version,"HTTP/1.1")!=0)
    {
        return BAD_REQUEST;
    }
    /*检查url是否合法*/
    if(strncasecmp(url,"http://",7)==0)
    {
        url+=7;
        url=strchr(url,'/');
    }
    if(!url || url[0]!='/')
    {
        return BAD_REQUEST;
    }
    printf("the request URL is:%s\n",url);
    /*HTTP 请求行处理完毕，状态转移到头部字段的分析*/
    checkstate=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/*分析头部字段*/
HTTP_CODE parse_headers(char*temp)
{
    /*遇到一个空行，说明我们得到了一个正确HTTP请求*/
    if(temp[0]='\0')
        return GET_REQUEST;
    else if(strncasecmp(temp,"Host:",5)==0)/*处理host头部字段*/
    {
        temp+=5;//向前5个位置，跳过host：
        temp+=strspn(temp," \t");//指针向前至没有空格
        printf("the request host is: %s\n",temp);
    }
    else /*其他头部字段不处理*/
    {
        printf("I can not handle the header\n");
    }
    return NO_REQUEST;
}
/*分析HTTP请求得入口函数*/
HTTP_CODE parse_content(char*buffer,int &checked_index,CHECK_STATE&checkstate,int &read_index,int & start_line)
{
    LINE_STATUS linestatus=LINE_OK;/*分析当前请求状态*/
    HTTP_CODE retcode=NO_REQUEST;/*计入HTTP请求结果*/
    /*主状态机，用于从buffer中取出所有完整的行*/
    while((linestatus=parse_line(buffer,checked_index,read_index))==LINE_OK)
    {
        char*temp=buffer+start_line;/*start_line是buffer起始位置*/
        start_line=checked_index;/*记录下一行起始位置*/
        /*记录主状态机的当前状态*/
        switch(checkstate)
        {
            case CHECK_STATE_REOUESTLINE: /*第一个状态，请求分析行*/
            {
                retcode=parse_requsestline(temp,checkstate);
                if(retcode==BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }    
            case CHECK_STATE_HEADER: /*第二个状态，分析头部字段*/
            {
                retcode=parse_headers(temp);
                if(retcode==BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(retcode==GET_REQUEST)
                {
                    return GET_REQUEST;
                }
                break;
            }
            default:
            {
                return INTERNAL_ERRON;
            }
        }
    }
    /*没有读取到一个完整的行，则表示还需要继续读取数据才能进一步分析*/
    if(linestatus==LINE_OPEN)
    {
        return NO_REQUEST;
    }
    else
    {
        return BAD_REQUEST;
    }
}

int main(int argc,char**argv)
{
    if(argc<=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
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
    struct sockaddr_in client_address;
    socklen_t client_addresslegth=sizeof(client_address);
    int fd=accept(listenfd,(struct sockaddr*)&client_address,&client_addresslegth );
    if(fd<0)
    {
        printf("errno is : %d\n ", errno);
    }
    else
    {
        char buffer[BUFFER_SIZE];/*读缓存区*/
        memset(buffer,'\0',BUFFER_SIZE);/*将buffer都变成空*/
        int data_read=0;/*读取的数据*/
        int read_index=0;/*当前已经读取的字节数*/
        int checked_index=0;/*已经分析完多少字节数据*/
        int start_line=0;/*行在buffer起始位置*/
        /*设置主状态机的初始状态*/
        CHECK_STATE checkstate=CHECK_STATE_REOUESTLINE;
        while (1)
        {
            data_read=recv(fd,buffer+read_index,BUFFER_SIZE-read_index,0);
            if(data_read==-1)
            {
                printf("reading failed\n");
                break;
            }
            else if(data_read==0)
            {
                printf("remote client has closed the connection\n");
                break;
            }
            read_index+=data_read;
            /*分析目前已经获得的所有客户端数据*/
            HTTP_CODE result=parse_content(buffer,checked_index,checkstate,read_index,start_line);
            if(result==NO_REQUEST)
            {
                continue;
            }
            else if(result==GET_REQUEST)
            {
                send(fd,szret[0],strlen(szret[0]),0);
                break;
            }
            else
            {
                send(fd,szret[1],sizeof(szret[1]),0);
                break;
            }
        }
        close(fd);
    }
    close(listenfd);
    return 0;
}