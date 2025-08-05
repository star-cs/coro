#include "coro/coro.hpp"

using namespace coro;

#define BUFFLEN 10240

task<> echo(int sockfd)
{
    // client等待read的时候，恢复
    char buf[BUFFLEN] = {0};
    int  ret          = 0;
    auto conn         = io::net::tcp::tcp_connector(sockfd); 

    while (true)
    {
        ret = co_await io::stdin_awaiter(buf, BUFFLEN, 0);  // 提交 I/O 任务，等待读取到 命令行 输入
        log::info("receive data from stdin: {}", buf);
        ret = co_await conn.write(buf, ret);
    }
}

task<> client(const char* addr, int port)
{
    auto client = io::net::tcp::tcp_client(addr, port);
    int  ret    = 0;
    int  sockfd = 0;
    sockfd      = co_await client.connect();
    assert(sockfd > 0 && "connect error");

    if (sockfd <= 0)
    {
        log::error("connect error");
        co_return;
    }

    // 协程内提交 子协程任务
    submit_to_scheduler(echo(sockfd));  // 当前协程还没释放，不会执行子协程。这里并没有记录父子协程的关系

    char buf[BUFFLEN] = {0};
    auto conn         = io::net::tcp::tcp_connector(sockfd);
    while ((ret = co_await conn.read(buf, BUFFLEN)) > 0)    // 协程 封装 I/O任务，协程suspend，等待read完成，句柄resume恢复
    {
        log::info("receive data from net: {}", buf);
    }

    ret = co_await conn.close();
    assert(ret == 0);
}

int main(int argc, char const* argv[])
{
    /* code */
    scheduler::init();
    submit_to_scheduler(client("localhost", 8000));

    scheduler::loop();
    return 0;
}
