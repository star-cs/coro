// echo_server.cpp
#include <liburing.h>       // io_uring 库头文件
#include <netinet/in.h>     // 套接字地址结构
#include <stdio.h>          // 标准输入输出
#include <string.h>         // 字符串操作
#include <unistd.h>         // POSIX 系统调用

#define EVENT_ACCEPT 0      // 接受连接事件类型
#define EVENT_READ 1        // 读取数据事件类型
#define EVENT_WRITE 2       // 写入数据事件类型

// 连接信息结构体（用于存储事件上下文）
struct conn_info {
    int fd;     // 文件描述符（套接字）
    int event;  // 事件类型（ACCEPT/READ/WRITE）
};

// 初始化 TCP 服务器
int init_server(unsigned short port) {
    // 创建 TCP 套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(struct sockaddr_in));
    
    // 配置服务器地址
    serveraddr.sin_family = AF_INET;            // IPv4
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网卡
    serveraddr.sin_port = htons(port);          // 端口号（主机字节序转网络字节序）

    // 绑定套接字
    if (-1 == bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr))) {
        perror("bind");
        return -1;
    }

    // 开始监听（最大挂起连接数为10）
    listen(sockfd, 10);

    return sockfd;  // 返回监听套接字
}

#define ENTRIES_LENGTH 1024  // io_uring 队列大小
#define BUFFER_LENGTH 1024   // 数据缓冲区大小

// 设置接收数据事件（READ）
void set_event_recv(struct io_uring *ring, int sockfd, void *buf, size_t len, int flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);  // 从环中获取空闲提交队列项

    // 创建连接信息（保存到 user_data 供完成事件使用）
    struct conn_info recv_info = {
        .fd = sockfd,
        .event = EVENT_READ,
    };

    // 准备接收操作：将接收请求填充到 SQE
    io_uring_prep_recv(sqe, sockfd, buf, len, flags);
    // 将连接信息复制到 SQE 的 user_data 字段（事件处理时用于识别）
    memcpy(&sqe->user_data, &recv_info, sizeof(struct conn_info));
}

// 设置发送数据事件（WRITE）
void set_event_send(struct io_uring *ring, int sockfd, void *buf, size_t len, int flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    struct conn_info send_info = {
        .fd = sockfd,
        .event = EVENT_WRITE,
    };

    // 准备发送操作
    io_uring_prep_send(sqe, sockfd, buf, len, flags);
    memcpy(&sqe->user_data, &send_info, sizeof(struct conn_info));
}

// 设置接受连接事件（ACCEPT）
void set_event_accept(struct io_uring *ring, int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    struct conn_info accept_info = {
        .fd = sockfd,
        .event = EVENT_ACCEPT,
    };

    // 准备接受连接操作
    io_uring_prep_accept(sqe, sockfd, addr, addrlen, flags);
    memcpy(&sqe->user_data, &accept_info, sizeof(struct conn_info));
}

int main(int argc, char *argv[]) {
    unsigned short port = 8000;  // 监听端口
    int sockfd = init_server(port);  // 初始化服务器套接字

    // 初始化 io_uring 参数
    struct io_uring_params params;
    memset(&params, 0, sizeof(params));
    
    // 创建 io_uring 实例
    struct io_uring ring;
    // 初始化提交队列和完成队列（长度为 ENTRIES_LENGTH）
    io_uring_queue_init_params(ENTRIES_LENGTH, &ring, &params);

    // 客户端地址结构（用于 accept）
    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
    
    // 初始提交一个 ACCEPT 事件（监听新连接）
    set_event_accept(&ring, sockfd, (struct sockaddr *)&clientaddr, &len, 0);

    char buffer[BUFFER_LENGTH] = {0};  // 数据缓冲区

    // 事件循环主逻辑
    while (1) {
        // 提交所有准备好的 SQE 到内核
        io_uring_submit(&ring);

        // 等待至少一个完成事件（阻塞调用）
        struct io_uring_cqe *cqe;
        io_uring_wait_cqe(&ring, &cqe);

        // 批量获取完成事件（最多128个）
        struct io_uring_cqe *cqes[128];
        int nready = io_uring_peek_batch_cqe(&ring, cqes, 128);

        // 处理所有就绪事件
        for (int i = 0; i < nready; i++) {
            struct io_uring_cqe *entry = cqes[i];
            struct conn_info result;
            // 从 user_data 提取事件上下文
            memcpy(&result, &entry->user_data, sizeof(struct conn_info));

            // 根据事件类型处理
            if (result.event == EVENT_ACCEPT) {
                // 重新提交 ACCEPT 事件（持续监听新连接）
                set_event_accept(&ring, sockfd, (struct sockaddr *)&clientaddr, &len, 0);
                
                int connfd = entry->res;  // accept 返回的新连接套接字
                if (connfd >= 0) {
                    // 为新连接注册 READ 事件
                    set_event_recv(&ring, connfd, buffer, BUFFER_LENGTH, 0);
                }
            } 
            else if (result.event == EVENT_READ) {
                int ret = entry->res;  // 读取的字节数
                
                if (ret == 0) {  // 客户端关闭连接
                    close(result.fd);
                } else if (ret > 0) {  // 成功读取数据
                    // 将接收到的数据原样注册为 WRITE 事件（echo）
                    set_event_send(&ring, result.fd, buffer, ret, 0);
                }
                // 注意：读取失败（ret<0）时直接关闭连接（此处未处理）
            } 
            else if (result.event == EVENT_WRITE) {
                // 写入完成后，重新注册 READ 事件等待后续数据
                set_event_recv(&ring, result.fd, buffer, BUFFER_LENGTH, 0);
            }
        }
        
        // 批量标记完成事件（释放 CQE 资源）
        io_uring_cq_advance(&ring, nready);
    }
    
    // 清理 io_uring 资源（实际不会执行到此处）
    io_uring_queue_exit(&ring);
    return 0;
}