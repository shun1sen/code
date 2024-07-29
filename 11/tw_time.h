#ifndef TIME_WHEEL_TIMER
#define TIME_WHEEL_TIMER

#include <time.h>
#include <netinet/in.h>
#include <stdio.h>

#define BUFFER_SIZE 64
class tw_timer;
/*绑定socket和定时器*/
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    tw_timer*timer;
};
class tw_timer
{
public:
    tw_timer(int rot,int ts):next(NULL),prve(NULL),rotation(rot),time_slot(ts){}
    
    int rotation;/*记录定时器在时间轮转多少圈后生效*/
    int time_slot;/*记录定时器属于时间轮上哪个槽*/
    void(*cb_func)(client_data*);/*定时器回调函数*/
    client_data*user_data;
    tw_timer*prve;
    tw_timer*next;
};

class time_wheel
{
public:
    time_wheel():cur_slot(0)
    {
        for(int i=0;i<N;i++)
            slots[i]=NULL;/*初始化每个槽的头结点*/
    }
    ~time_wheel()
    {
        for(int i=0;i<N;i++)
        {
            tw_timer*temp=slots[i];
            while(temp)
            {
                slots[i]=temp->next;
                delete temp;
                temp=slots[i];
            }
        }
    }
    /*根据定时器timeout创建一个定时器，并把它插入到合适的槽中*/
    tw_timer* add_timer(int timeout)
    {
        if(timeout<0)
            return NULL;
        int ticks=0;
        /*下面根据待插入定时器的超时值计算它在时间轮转动多少个滴答后被触发，并将该滴答数存储于变量ticks中。
        如果待插入定时器的超时值小于时间轮的槽间隔SI，则将ticks向上折合为1，否则就将ticks向下折合为timeout/SI*/
        if(timeout<SI)
        {
            ticks=1;
        }
        if(timeout>SI)
        {
            ticks=timeout/SI;
        }
        /*计算待插入的定时器在时间轮转动多少圈后被触发*/
        int rotation=ticks/N;
        /*计算待插入的定时器应该插入哪个槽中*/
        int ts=(cur_slot+(ticks%N))%N;
        /*创建新的定时器，它在时间轮抓你爹rotation圈后被触发，且位于ts个槽上*/
        tw_timer*timer=new tw_timer(rotation,ts);
        /*如果第ts个槽中尚无任何定时器，则新建的定时器为该槽的头节点*/
        if(!slots[ts])
        {
            printf("add timer,rotation is %d,ts is %d,cur_slot is %d\n ",rotation,ts,cur_slot);
            slots[ts]=timer;
        }
        /*否则插入第ts槽的链表中*/
        else{
            timer->next=slots[ts];
            slots[ts]->prve=timer;
            slots[ts]=timer;
        }
        return timer;
    }
    /*s删除目标定时器*/
    void del_timer(tw_timer*timer)
    {
        if(!timer)
            return;
        int ts=timer->time_slot;
        /*如果timer是ts槽的头节点，则需要重置头节点*/
        if(timer==slots[ts])
        {
            slots[ts]==timer->next;
            if(slots[ts])
            {
                slots[ts]->prve=NULL;
            }
            delete timer;
        }
        else
        {
            timer->prve->next=timer->next;
            if(timer->next)
                timer->next->prve=timer->prve;
            delete timer;
        }
    }
    /*SI时间到后，时间轮向前滚动一个槽的间隔*/
    void tick()
    {
        tw_timer*temp=slots[cur_slot];
        printf("current slot is %d\n",cur_slot);
        while(temp)
        {
            printf("tick  the timer once\n");
            /*如果定时器的rotation值大于0，则它在这一轮不起作用*/
            if(temp->rotation>0)
            {
                temp->rotation--;
                temp=temp->next;
            }
            /*否则，说明定时器已经到期，开始指向定时到了的任务，然后删除该定时器*/
            temp->cb_func(temp->user_data);
            if(temp==slots[cur_slot])//链表首部
            {
                printf("detele header in cur_slot\n");
                slots[cur_slot]=temp->next;
                delete temp;
                if(slots[cur_slot])
                {
                    slots[cur_slot]->prve=NULL;
                }
                temp=slots[cur_slot];
            }
            else
            {
                temp->prve->next=temp->next;
                if(temp->next)
                    temp->next->prve=temp->prve;
                tw_timer*temp2=temp->next;
                delete temp;
                temp=temp2;
            }
        }
        cur_slot=(cur_slot+1)%N;
    }
private:
    static const int N=60;/*时间轮槽数*/
    static const int SI=1;/*每1s转到一次，即槽间隔为1s*/
    tw_timer*slots[N];/*时间轮的槽数组，每个槽指向一个链表*/
    int cur_slot;/*时间轮当前槽*/
};
#endif