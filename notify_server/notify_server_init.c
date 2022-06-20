#include <stdio.h>  
#include <sys/types.h>  
#include <sys/socket.h>  
#include <sys/un.h>   
   
#define CAN_SERVICE "CAN_SERVICE" 
int main(void)  
{      
    int ret;     
    int len;  
    int accept_fd;
    int socket_fd; 
    static char recv_buf[1024];
    socklen_t clt_addr_len; 
    struct sockaddr_un clt_addr;  
    struct sockaddr_un srv_addr;  
 
    socket_fd=socket(PF_UNIX,SOCK_STREAM,0);  
    if(socket_fd<0)  
    {  
        perror("cannot create communication socket");  
        return 1;  
    }    
          
    // 设置服务器参数  
    srv_addr.sun_family=AF_UNIX;  
    strncpy(srv_addr.sun_path,CAN_SERVICE,sizeof(srv_addr.sun_path)-1);  
    unlink(CAN_SERVICE);  
 
    // 绑定socket地址 
    ret=bind(socket_fd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));  
    if(ret==-1)  
    {  
        perror("cannot bind server socket");  
        close(socket_fd);  
        unlink(CAN_SERVICE);  
        return 1;  
    }  
 
    // 监听   
    ret=listen(socket_fd,1);  
    if(ret==-1)  
    {  
        perror("cannot listen the client connect request");  
        close(socket_fd);  
        unlink(CAN_SERVICE);  
        return 1;  
    }  
 
    // 接受connect请求 
    len=sizeof(clt_addr);  
    accept_fd=accept(socket_fd,(struct sockaddr*)&clt_addr,&len);  
    if(accept_fd<0)  
    {  
        perror("cannot accept client connect request");  
        close(socket_fd);  
        unlink(CAN_SERVICE);  
        return 1;  
    }  
 
    // 读取和写入  
    memset(recv_buf,0,1024);  
    int num=read(accept_fd,recv_buf,sizeof(recv_buf));  
    printf("Message from client (%d)) :%s\n",num,recv_buf);    
         
    // 关闭socket
    close(accept_fd);  
    close(socket_fd);  
    unlink(CAN_SERVICE);  
    return 0;  
}