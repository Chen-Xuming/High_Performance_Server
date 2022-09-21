//
// Created by chen on 2022/9/20.
//

// 基于双链表的升序定时器

#ifndef LINUX_HIGH_PERFORMANCE_SERVER_LIST_TIMER_H
#define LINUX_HIGH_PERFORMANCE_SERVER_LIST_TIMER_H

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdio>

#define BUFFER_SIZE 64
class util_timer;

// 用户数据
struct client_data{
    sockaddr_in address;
    int sock_fd;
    char buf[BUFFER_SIZE];
    util_timer *timer;
};

// 定时器类（节点）
class util_timer{
public:
    time_t expire;                      // 任务超时时间（绝对时间），long int
    void (*cb_func) (client_data *);    // 任务回调函数，参数为client_data *，无返回值
    client_data *user_data = NULL;      // 回调函数处理的用户数据
    util_timer *prev, *next;

    util_timer(): prev(NULL), next(NULL){}
};

// 定时器链表: 带头尾节点的升序双链表
class sort_timer_list{
public:
    sort_timer_list(): head(NULL), tail(NULL){}
    ~sort_timer_list(){
        // 销毁所有定时器节点
        util_timer *tmp = head;
        while(tmp){
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    // 添加定时器
    void add_timer(util_timer *timer){
        if(timer == NULL) return;
        if(head == NULL) {
            head = tail = timer;
            return;
        }

        // 如果timer比当前所有定时器时间小，直接头插
        if(timer->expire < head->expire){
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }

        // 否则，插入到正确的位置
        add_timer(timer, head);
    }

    // 如果某个定时器发生变化，调整对应的定时器在链表中的位置
    // 这里只考虑超时时间延长的情况，即只可能向后移动
    void adjust_timer(util_timer *timer){
        if(timer == nullptr) return;

        util_timer *tmp = timer->next;
        if(tmp == nullptr || timer->expire < tmp->expire) return;

        // 如果它是head，那么直接取出重新插入
        if(timer == head){
            head = head->next;
            head->prev = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }

        // 如果不是head，取出插入到其后面的链表
        else{
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    // 删除某个节点
    void del_timer(util_timer *timer){
        if(timer == nullptr) return;
        if(timer == head && timer == tail){
            delete timer;
            head = nullptr;
            tail = nullptr;
            return;
        }
        if(head == timer){
            head = head->next;
            head->prev = nullptr;
            delete timer;
            return;
        }
        if(timer == tail){
            tail = tail->prev;
            tail->next = nullptr;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    // SIGALRM信号每次被触发就在其信号处理函数中执行一次tick函数，处理链表上到期的任务
    void tick(){
        if(head == nullptr) return;
        printf("timer tick.\n");
        time_t cur = time(NULL);    // 当前系统时间
        util_timer *tmp = head;

        // 处理所有到期任务
        while(tmp){
            if(cur < tmp->expire){  // 剩下的任务都是还未到期的
                break;
            }
            tmp->cb_func(tmp->user_data);
            head = tmp->next;   // 删除已处理完的任务
            if(head){
                head->prev = nullptr;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    // 将定时器插入到正确的位置
    void add_timer(util_timer *timer, util_timer *lst_head){
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;

        // 把timer插入到比它大的定时器节点之前
        while(tmp){
            if(timer->expire < tmp->expire){
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = prev->next;
        }

        // 如果搜索完整条链都没有找到比它大的，那么插入到最后
        if(tmp == NULL){
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }

private:
    // 头尾节点（注意，它们不是虚节点）
    util_timer *head;
    util_timer *tail;
};


#endif //LINUX_HIGH_PERFORMANCE_SERVER_LIST_TIMER_H
