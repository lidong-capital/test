/*
 * @Descripttion: 
 * @version: 0.0.0.1
 * @Author: lidong 
 * @Date: 2020-06-11 16:01:45
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2020-06-15 15:00:34
 */ 
#pragma once

#include <string>
#include "interface_log.h"
#include "my_shm.h"
#include "utils.h"
#include "net_epoll.h"
#include <unordered_map>
#include <errno.h>
#include <thread>
#include "my_om.h"

typedef struct factor_msg_head  
{
    unsigned int  type;   // 消息类型
    unsigned int  size;   // 单个消息大小,可以用来判断消息包的合法性
}factor_msg_head_t;

// 传输因子数据
typedef struct factor_pub
{
     struct factor_msg_head head;
     my_factor data;
}factor_pub_t;

// 应答结果类型
enum req_result{
    req_fail = -1,          // 请求失败
    req_success = 0,        // 请求成功
    req_not_define = 1,     // 请求功能未定义
};

// 请求订阅时的结构体
typedef struct factor_subscribe{
    char chType;            // 操作类型 暂定只有0 订阅
    char szSublist[256];    // 订阅列表
}factor_subscribe_t;

// 应答信息结构体
typedef struct ret_msg
{
    int ret_code;
    char ret_msg[64];
}ret_msg_t;

// 应答数据
typedef struct factor_ret_msg
{
    factor_msg_head_t head;
    ret_msg_t data;
}factor_ret_msg_t;


// 订阅节点
typedef struct st_list_node{
    int socketfd;
    struct st_list_node * prev;
    struct st_list_node * next;
}st_list_node;

class factor_server 
{    
public:    
    explicit factor_server(const char* ip, const char* port);
    virtual ~factor_server();
    void run();

private:
    void proc_net();
    void proc_data();
    int init();

private:
    net_epoll_cfg net_cfg_;
 
    //my_quote_write* handler_;
    //my_quote_read* pread_;
};
  
