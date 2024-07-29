#include<unistd.h>
#include<stdio.h>
int main()
{
    pid_t pid=getpid();
    pid_t pgid=getpgid(pid);
    uid_t uid=getuid();
    uid_t euid=geteuid();
    printf("userid is %d,effective userid is : %d \n",uid,euid);
    printf("process id is %d,process group id is:%d\n",pid,pgid);
    return 0;
}