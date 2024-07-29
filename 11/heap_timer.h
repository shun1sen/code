#ifndef MIN_HEAP
#define MIN_HEAP

#include<iostream>
#include<netinet/in.h>
#include<time.h>
using std::exception;

#define BUFFER_SIZE 64
class heap_timer;
/*绑定socket和定时器*/
struct client_data
{
    sockaddr_in address;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer*timer;
};
class heap_timer
{
public:
    heap_timer(int delay)
    {
        expire=time(NULL)+delay;
    }
    time_t expire;
    void(*cu_fun)(client_data*);
    client_data*user_data;
};

/*时间堆类*/
class time_heap
{
private:
    heap_timer**array;/*堆数组*/
    int capacity;/*堆数组容量*/
    int cur_size;/*堆数组当前包含的元素个数*/
    void percolate_down(int hole)/*最小堆下滤操作，它确保堆数组中以第hole个节点作为根的子树拥有最小堆的性质*/
    {
        heap_timer*temp=array[hole];
        int child=0;
        for(;((hole*2+1)<=(cur_size-1));hole=child)
        {
            child=hole*2+1;
            if((child<(cur_size-1))&&(array[child+1]->expire<array[child]->expire))
            {
                ++child;
            }
            if(array[child]->expire<temp->expire)
            {
                array[hole]=array[child];
            }
            else
                break;
        }
        array[hole]=temp;
    }
    /*将堆数组容量扩大一倍*/
    void resize() 
    {
        heap_timer**temp=new heap_timer*[2*capacity];
        for(int i=0;i<2*capacity;i++)
            temp[i]=NULL;
        if(!temp)
            throw std::exception();
        capacity=2*capacity;
        for(int i=0;i<cur_size;i++)
            temp[i]=array[i];
        delete [] array;
        array=temp;

    }
public:
    /*构造函数1，初始化一个大小为cap的空堆*/
    time_heap(int cap) :capacity(cap),cur_size(0)
    {
        array=new heap_timer*[capacity];
        if(!array)
        {
            throw std::exception();
        }
        for(int i=0;i<capacity;i++)
        {
            array[i]=NULL;
        }
    }
    /*构造函数2，用已有的数组来初始化堆*/
    time_heap(heap_timer**init_array,int size,int capacity) :cur_size(size),capacity(capacity)
    {
        if(capacity<size)
        {
            throw std::exception();
        }
        array=new heap_timer*[capacity];
        if(!array)
        {
            throw std::exception();
        }
        for(int i=0;i<capacity;i++)
        {
            array[i]=NULL;
        }
        if(size!=0)
        {
            for(int i=0;i<size;i++)
                array[i]=init_array[i];
            for(int i=(cur_size-1)/2;i>=0;i--)
                percolate_down(i);
        }
    }
    /*析构函数，注销时间堆*/
    ~time_heap()
    {
        for(int i=0;i<cur_size;i++)
            delete array[i];//避免内存泄漏
        delete [] array;
    }

    /*添加目标定时器*/
    void add_timer(heap_timer*timer) 
    {
        if(!timer)
        {
            return;
        }
        if(cur_size>=capacity)//扩大内存
        {
            resize();
        }
        /*新插入了一个元素，当前堆大小加1，hole是新建空穴的位置*/
        int hole=cur_size++;
        int parent=0;
        /*对从空穴到根节点的路径上所有节点执行上滤操作*/
        for(;hole>0;hole=parent)
        {
            parent=(hole-1)/2;
            if(array[parent]->expire<=timer->expire)
            {
                break;
            }
            else
            {
                array[hole]=array[parent];//空结点赋值为array[parent],即parent下降到hole位置
            }
        }
        array[hole]=timer;
    }
    void del_timer(heap_timer*timer)
    {
        if(!timer)
            return;
        /*仅仅将目标定时器的回调函数设置为空，即所谓的延迟销毁，这将节省真正删除该定时器造成的开销，但这样容易造成数组膨胀*/
        timer->cu_fun=NULL;
    }

    heap_timer*top() const
    {
        if(cur_size==0)
        {
            return NULL;
        }
        return array[0];
    }

    void pop_timer()
    {
        if(cur_size==0)
            return;
        if(array[0])
        {
            delete array[0];
            array[0]=array[--cur_size];
            percolate_down(0);
        }
    }

    /*心搏函数*/
    void tick()
    {
        heap_timer*temp=array[0];
        time_t cur=time(NULL);
        while(cur_size!=0)
        {
            if(!temp)
                break;
            /*如果堆顶定时器没有到期，则退出循环*/
            if(temp->expire>cur)
                break;
            if(array[0]->cu_fun)
            {
                array[0]->cu_fun(array[0]->user_data);
            }
            pop_timer();
            temp=array[0];
        }
    }
};
#endif
