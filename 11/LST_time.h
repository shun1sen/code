#ifndef LST_TIMER
#define LST_TIMER
#include<netinet/in.h>
#include<time.h>
#include<stdio.h>
#define BUFFER_SIZE 64
class util_timer;
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer*timer;
};
class util_timer
{
public:
    util_timer():prve(NULL),next(NULL){}
    time_t expire;/*任务超时时间，绝对时间*/
    void(*cb_func)(client_data*);/*任务回调函数*/
    client_data*user_data;
    util_timer*prve;
    util_timer*next;
};
/*定时器链表*/
class sort_timer_lst
{
public:
    sort_timer_lst():head(NULL),tail(NULL){};
    ~sort_timer_lst()
    {
        util_timer*temp=head;
        while(temp)
        {
            head=temp->next;
            delete temp;
            temp=head;
        }
    }
        /*添加定时器*/
    void add(util_timer*timer)
    {
        if(!timer)
        {
            return;
        }
        if(!head)
        {
            head=timer=tail;
            return;
        }
        if(timer->expire<head->expire)
        {   
            timer->next=head;
            head->prve=timer;
            head=timer;
            return;
        }
        add_timer(timer,head);
    }      
    /*当定时任务发送变化，调整在定时器链表的位置,只考虑时间增加情况*/
    void adjust(util_timer*timer)
    {
        if(!timer)
        {
            return;
        }
        util_timer*temp=timer->next;
        /*为末尾或时间任何仍小于后面*/
        if(!temp||timer->expire<temp->expire)
        {
            return;
        }
        /*时间改变后大于后面*/
        /*为头部*/
        if(timer==head)
        {
            head=head->next;
            head->prve=NULL;
            timer->next=NULL;
            add_timer(timer,head);
        }
        /*其他位置*/
        else
        {
            timer->prve->next=timer->next;
            timer->next->prve=timer->prve;
            add_timer(timer,timer->next);
        }
    }
    void del_timer(util_timer*timer)
    {
        if(!timer)
        {
            return;
        }
        if((timer==head)&&(head==tail))
        {
            delete timer;
            head==NULL;
            tail=NULL;
            return;
        }
        if(timer==head)
        {
            head=timer->next;
            head->prve=NULL;
            delete timer;
            return;
        }
        if(timer==tail)
        {
            tail=timer->prve;
            tail->next=NULL;
            delete timer;
            return;
        }
        timer->prve->next=timer->next;
        timer->next->prve=timer->prve;
        delete timer;
    }
    /*从头结点开始依次处理每个定时器，直到遇到一个尚未到期的定时器，这就是定时器
的核心逻辑*/
    void tick()
    {
        if(!head)
        {
            return;
        }
        printf("time tick\n");
        time_t cur=time(NULL);
        util_timer*temp=head;
        while(temp)
        {
            if(cur<temp->expire)
                break;
            temp->cb_func(temp->user_data);
            head=temp->next;
            if(head)
            {
                head->prve=NULL;
            }
            delete temp;
            temp=head;
        }
    }
private:
    void add_timer(util_timer*timer,util_timer*lst_head)
    {
        util_timer*prve=lst_head;
        util_timer*temp=prve->next;
        while(temp)
        {
            if(timer->expire<temp->expire)
            {
                prve->next=timer;
                timer->next=temp;
                temp->prve=timer;
                timer->prve=prve;
                break;
            }
            prve=temp;
            temp=temp->next;
        }
    }
    util_timer*head;
    util_timer*tail;
};



#endif