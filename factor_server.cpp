#include "factor_server.h"
//test
my_shm_read<my_factor_record>* g_factor_sub;
std::vector<int> g_factor_fds_;
pthread_spinlock_t g_factor_lock;
std::unordered_map <long, st_list_node> unorderMap_list;

// 建立客户端连接
int quote_factor_accept(int fd)                      
{
    auto it = find(g_factor_fds_.begin(), g_factor_fds_.end(), fd);
    if (it == g_factor_fds_.end()){
        g_factor_fds_.push_back(fd);
    }
    MY_INFO("%s: fd %d", __FUNCTION__, fd);           
    return 0;
}

// 删除所有订阅节点
void delet_all_node(st_list_node* head)
{
    if(NULL == head || NULL == head->next)
   {
       return;
   }

   st_list_node* cur_node = head->next;
   st_list_node* next_node = cur_node;
   while(cur_node != NULL)
   {
       next_node = cur_node->next; 
       delete cur_node;
       cur_node = next_node;
   }
}

// 删除指定的订阅节点
void delet_node(st_list_node* head, int socketfd)
{  
   if(NULL == head || NULL == head->next)
   {
       return;
   }

   st_list_node  *cur_node = head->next;
   while(cur_node != NULL && cur_node->socketfd != socketfd)
   {
       cur_node = cur_node->next;
   }
   if(cur_node != NULL && cur_node->socketfd == socketfd)
   {
       if (cur_node->next == NULL)
       {
           cur_node->prev->next = NULL;
       }
       else
       {
           cur_node->prev->next = cur_node->next;
           cur_node->next->prev = cur_node->prev;
       }
       printf("fd %d close subscribe\n", socketfd);
       delete cur_node;
   }
}

// 关闭连接
int quote_factor_close(int fd)
{
    close(fd);
    std::vector<int>::iterator itr;
    for(itr=g_factor_fds_.begin(); itr!=g_factor_fds_.end();){
        if (fd == *itr){
            g_factor_fds_.erase(itr);
            break;
        } else{
            itr++;
        }
    }

    // 删除此客户端所在订阅列表
    std::unordered_map<long , st_list_node>::iterator itrSub;
    for(itrSub = unorderMap_list.begin(); itrSub != unorderMap_list.end(); itrSub++)
    {
        delet_node(&itrSub->second, fd);
    }
    MY_INFO("%s: fd[%d] close", __FUNCTION__, fd);
    return 0;
}

void add_factor(int p_iFactor)  // 添加因子
{
    if (0 == unorderMap_list.count(p_iFactor))
    {
        st_list_node list_head;
        list_head.next  = NULL;
        list_head.prev = NULL;
        list_head.socketfd = -1;
        unorderMap_list[p_iFactor] = list_head;
        printf("add factor %d\n", p_iFactor); 
    }
    return;
}

// 查找订阅列表是否有此信息
bool is_exist_fd(st_list_node* head, int fd)
{
    if (NULL == head || NULL == head->next)
    {
        return false;
    }

    st_list_node *cur_node = head->next;
    while (cur_node != NULL)
    {
        if(cur_node->socketfd == fd)
        {
            return true;
        }
        cur_node = cur_node->next;
    }
    return false;
}

// 添加订阅列表
void add_sub_list(int fd, int factor)
{
    st_list_node *node = new st_list_node;
    if(node == NULL)
    {
        MY_ERROR("creat list node fail");
        return;
    }
    node->socketfd = fd;
    node->next = NULL;
    node->prev = NULL;
    
    auto itr = unorderMap_list.find(factor);
    if (itr != unorderMap_list.end())
    {
        // 存在订阅则不重新添加
        if(is_exist_fd(&itr->second, fd))
        {
            return;
        }
        if(itr->second.next == NULL)
        {
            itr->second.next = node;
            node->prev = &(itr->second);
        }
        else
        {
            node->next = itr->second.next;
            itr->second.next = node;
            node->next->prev = node;
            node->prev = &(itr->second);
        }
        printf("fd %d add factor %d\n", fd, factor); 
    }
    return;
}

// 处理请求中的订阅列表
int handle_sub(int fd, const char *sub_list)
{
    std::vector<std::string> data;
    split_string(sub_list, "|", data);
    for (unsigned int i = 0; i < data.size(); ++i)
    {
        int factor_num = atoi(data[i].c_str());
        add_factor(factor_num);
        add_sub_list(fd, factor_num);
    }
    return 0;
}

int hadle_msg(int fd, char *msg_sub, ssize_t msg_size)
{
    factor_subscribe_t *ptr_factor = (factor_subscribe_t *)msg_sub;

    int ret_code = req_success;

    // 请求处理
    switch (ptr_factor->chType)
    {
        //订阅
    case '0':
        handle_sub(fd, ptr_factor->szSublist);
        MY_INFO("fd %d recv subscribe list %s", fd, ptr_factor->szSublist);
        ret_code = req_success;
        break;
    default:
        MY_ERROR("fd %d request %c is not define ", fd, ptr_factor->chType);
        ret_code = req_not_define;
        break;
    }

    // 应答数据
    factor_ret_msg_t msg;
    msg.head.type = QUOTE_FACTOR_SUBSCRIBE;
    msg.head.size = sizeof(ret_msg);
    switch (ret_code)
    {
    case req_fail:
        msg.data.ret_code = req_fail;
        strncpy(msg.data.ret_msg, "Request fail", sizeof(msg.data.ret_msg));
        printf("Request fail \n");
        MY_ERROR("Request fail");
        break;
    case req_success:
        msg.data.ret_code = req_success;
        strncpy(msg.data.ret_msg, "Request accepted", sizeof(msg.data.ret_msg));
        printf("Request accepted \n");
        MY_INFO("Request accepted");
        break;
    case req_not_define:
        msg.data.ret_code = req_not_define;
        strncpy(msg.data.ret_msg, "This function is not supported temporarily", sizeof(msg.data.ret_msg));
        printf("This function is not supported temporarily\n");
        MY_ERROR("This function is not supported temporarily");
        break;
    default:
        break;
    }
    pthread_spin_lock(&g_factor_lock);
    if (-1 == net_send(fd, (char *)&msg, sizeof(factor_ret_msg_t)))
    {
        printf("send ret_msg fd(%d) was broken, %s\n", fd, strerror(errno));
        MY_ERROR("send ret_msg fd(%d) was broken, %s", fd, strerror(errno));
    }
    pthread_spin_unlock(&g_factor_lock);
    return 0;
}

int quote_factor_proc(int fd, char* net_msg, ssize_t size)
{
    if (fd){
        hadle_msg(fd, net_msg, size);
    }
    return 0;
}


factor_server::factor_server(const char* ip, const char* port)
{
    pthread_spin_init(&g_factor_lock, PTHREAD_PROCESS_SHARED); 
    g_factor_fds_.clear();
    unorderMap_list.clear();

    net_cfg_.ip = ip;
    net_cfg_.port = port;
    net_cfg_.accept = quote_factor_accept;     // 接收连接
    net_cfg_.proc = quote_factor_proc;         // 接收数据  
    net_cfg_.close = quote_factor_close;

    my_log->proc();
}

factor_server::~factor_server()
{
    for (auto fd : g_factor_fds_){
        close(fd);
    }
    g_factor_fds_.clear();

    // 删除订阅列表
    std::unordered_map<long, st_list_node>::iterator itrSub;
    for(itrSub = unorderMap_list.begin(); itrSub != unorderMap_list.end(); itrSub++)
    {
        delet_all_node(&itrSub->second);
        itrSub->second.next = NULL;
    }
    unorderMap_list.clear();
}

int factor_server::init()
{
    char* shm = my_shm_open(MY_QUEUE_USER_WRITER, g_kepler_cfg.shms.shm_size);
    g_factor_sub = new my_shm_read<my_factor_record>(shm, g_kepler_cfg.shms.factor.size, get_offset(SHM_TYPE_FACTOR, 0),SHM_TYPE_FACTOR);
    if(NULL == g_factor_sub)
    {
         printf("creat my_shm_read fail\n");
         MY_ERROR("creat my_shm_read fail, exit!");
         return -1;
    }
    else
    {
        printf("creat my_shm_read success\n");
        MY_INFO("creat my_shm_read success");
    }
    return 0;
}

void factor_server::proc_net()
{
    net_epoll_run(&net_cfg_);
}

void factor_server::run()
{
    int ret = init();
    if(ret != 0)
    {
        printf("init fail\n");
        return;
    }
    std::thread t1(std::bind(&factor_server::proc_data,this));
    t1.detach();
    std::thread t2(std::bind(&factor_server::proc_net,this));
    t2.detach();
    return ;
}

void factor_server::proc_data()
{
    my_factor_record *pf;
    while (g_factor_sub)
    {
        while (g_factor_sub->pull(pf))
        {
            // 如果没有任何客户端订阅此因子，则直接丢弃
            auto itr = unorderMap_list.find(pf->data.fid);
            if (itr != unorderMap_list.end())
            {
                if(NULL == itr->second.next)
                {
                    continue;
                }
                st_list_node *list_node = itr->second.next;
                factor_pub_t pub_factor = {0};
                pub_factor.head.type = QUOTE_FACTOR_INFO;
                pub_factor.head.size = sizeof(my_factor);
                memcpy(&pub_factor.data, &(pf->data), sizeof(my_factor));
                while (list_node != NULL)
                {
                    pthread_spin_lock(&g_factor_lock);
                    // socketfd 发布数据
                    if (-1 == net_send(list_node->socketfd, (char *)&pub_factor, sizeof(pub_factor)))
                    {
                        printf("send my_factor fd(%d) was broken, %s\n", list_node->socketfd, strerror(errno));
                        MY_ERROR("send my_factor fd(%d) was broken, %s", list_node->socketfd, strerror(errno));
                    }
                    else
                    {
                        MY_DEBUG("send my_factor[%ld] fd[%d] success", pf->data.fid, list_node->socketfd);
                    }
                    list_node = list_node->next;
                    pthread_spin_unlock(&g_factor_lock);
                }
            }   
        }
    }
    return;
}
