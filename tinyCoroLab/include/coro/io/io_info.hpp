#pragma once

#include <coroutine>
#include <cstdint>
#include <functional>

namespace coro::io::detail
{
#define CASTPTR(data)  reinterpret_cast<uintptr_t>(data)
#define CASTDATA(data) static_cast<uintptr_t>(data)

struct io_info;

using std::coroutine_handle;
using cb_type = std::function<void(io_info*, int)>;

enum io_type
{
    nop,
    tcp_accept,
    tcp_connect,
    tcp_read,
    tcp_write,
    tcp_close,
    stdin,
    timer,
    none
};

struct io_info
{
    coroutine_handle<> handle; // IO 绑定的协程句柄
    int32_t            result; // IO 执行完的结果
    io_type            type;   // IO 类型
    uintptr_t          data;   // IO 绑定的内存区域
    cb_type            cb;     // IO 绑定的回调函数
};

inline uintptr_t ioinfo_to_ptr(io_info* info) noexcept
{
    return reinterpret_cast<uintptr_t>(info);
}

inline io_info* ptr_to_ioinfo(uintptr_t ptr) noexcept
{
    return reinterpret_cast<io_info*>(ptr);
}

}; // namespace coro::io::detail