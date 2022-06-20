#include "notify_server_init.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/un.h>   
   
static int g_IsNotifyServerInit = 0;
static int g_socketFd = -1;

static void CloseServerSocket(void)
{
    close(g_socketFd);
    g_socketFd = -1;
}

int GetClientSocket(void)
{
    return g_socketFd;
}

static int CreateServerSocket(void)
{
    struct sockaddr_un addr;
    memset((void *)&addr, 0, sizeof(addr));
    g_socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socketFd < 0) {
        NOTIFY_LOG_ERROR("cannot create client socket");  
        return -1;  
    }
    addr.sun_family = AF_UNIX;
    int ret = snprintf(addr.sun_path, sizeof(addr.sun_path), "notify_server");
    if(ret < 0) {
        NOTIFY_LOG_ERROR("snprintf error ret %d", (int)errno);
        CloseServerSocket();
        return -1;
    }
    unlink(addr.sun_path);
    ret = bind(g_socketFd, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        NOTIFY_LOG_ERROR("bind faild ret %d", (int)errno);
        CloseServerSocket();
        return -1;
    }
    return 0;
}

int main(void)  
{      
    int ret;     
    int len;  
    int accept_fd;
    int g_socketFd; 
    static char recv_buf[1024];
    socklen_t clt_addr_len; 
    struct sockaddr_un clt_addr;  
    struct sockaddr_un srv_addr;  
    if (CreateServerSocket() < 0) {
        NOTIFY_LOG_ERROR("Create Client Socket failed");
        return -1;
    }
    // 监听   
    ret=listen(g_socketFd,1);  
    if(ret==-1)  
    {  
        perror("cannot listen the client connect request");  
        close(g_socketFd);  
        unlink(CAN_SERVICE);  
        return 1;  
    }  
 
    // 接受connect请求 
    len=sizeof(clt_addr);  
    accept_fd=accept(g_socketFd,(struct sockaddr*)&clt_addr,&len);  
    if(accept_fd<0)  
    {  
        perror("cannot accept client connect request");  
        close(g_socketFd);  
        unlink(CAN_SERVICE);  
        return 1;  
    }  
 
    // 读取和写入  
    memset(recv_buf,0,1024);  
    int num=read(accept_fd,recv_buf,sizeof(recv_buf));  
    printf("Message from client (%d)) :%s\n",num,recv_buf);    
         
    // 关闭socket
    close(accept_fd);  
    close(g_socketFd);  
    unlink(CAN_SERVICE);  
    return 0;  
}

int NotifyServerInit(void)
{
    if (g_IsNotifyServerInit == 1) {
        return -1;
    }
    if (CreateServerSocket() < 0) {
        NOTIFY_LOG_ERROR("Create Client Socket failed");
        return -1;
    }
    if (CreateRecvThread() < 0) {
        NOTIFY_LOG_ERROR("Create recive thread failed");
        CloseServerSocket();
        DestroyThreadPool();
        return -1;
    }
    NOTIFY_LOG_INFO("init success");
    g_IsNotifyServerInit = 1;
    return 0;
}