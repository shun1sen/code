#include<sys/sem.h>//信号量API
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>//wait函数和waitpid函数

union semun
{
    int val;
    struct semid_ds*buf;
    unsigned short int*array;
    struct seminfo * _buf;
};
/*op为-1即减一操作执行P操作，为1执行V操作*/
void PV(int sem_id,int op)
{
    struct sembuf sem_b;
    sem_b.sem_num=0;
    sem_b.sem_op=op;
    sem_b.sem_flg=SEM_UNDO;
    semop(sem_id,&sem_b,1);
}

int main(int argc,char*argv[])
{
    int sem_id=semget(IPC_PRIVATE,1,0666);
    union semun sem_un;
    sem_un.val=1;
    semctl(sem_id,0,SETVAL,sem_un);
    pid_t id=fork();
    if(id<0)
    {
        return 1;
    }
    else if(id==0)
    {
        printf("child try to get binary sem\n");
        PV(sem_id,-1);
        printf("child get the sem and would release it after 5 seconds\n");
        sleep(5);
        PV(sem_id,1);
        exit(0);
    }
    else
    {
        printf("parent try to get binary sem\n");
        PV(sem_id,-1);
        printf("parent get the sem and would release it after 5 seconds\n");
        sleep(5);
        PV(sem_id,1);
        //exit(0);
    }
    waitpid(id,NULL,0);//NULL 表示不关心子进程的退出状态，0 表示不设置任何选项，因此这个调用会阻塞，直到指定的子进程状态改变。
    semctl(sem_id,0,IPC_RMID,sem_un);/*删除信号量*/
    return 1;
}

