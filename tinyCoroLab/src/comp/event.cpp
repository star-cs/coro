#include "coro/comp/event.hpp"
#include "coro/detail/types.hpp"
#include "coro/scheduler.hpp"
#include <atomic>

namespace coro
{
// TODO[lab4a] : Add codes if you need

namespace detail
{

auto event_base::awaiter_base::await_ready() noexcept -> bool
{
    m_ctx.register_wait(); // 表示该上下文有一个协程任务 阻塞在外，防止context析构。
    return m_ev.is_set();
}

auto event_base::awaiter_base::await_suspend(std::coroutine_handle<> handle) noexcept -> bool
{
    m_await_coro = handle;
    // 将协程 awaiter 挂载到 event 中，这个过程期间有可能 event 被 set，因此返回 bool 表示是否
    // 挂载成功，挂载失败那么协程恢复运行
    return m_ev.register_awaiter(this);
}

// context重新调度，执行await_resume()，减少引用计数。
auto event_base::awaiter_base::await_resume() noexcept -> void
{
    m_ctx.unregister_wait();
}

auto event_base::set_state() noexcept -> void // 设置 event，唤醒所有 suspend awaiter
{
    auto flag = m_state.exchange(this, std::memory_order_acq_rel);
    if (flag != this) // event 之前未被 set，flag为m_state的旧值。flag就是所有待恢复的 awaiter_base*
    {
        auto waiter = static_cast<awaiter_base*>(flag);
        resume_all_awaiter(waiter); // 取得列表头并恢复所有 suspend awaiter
    }
}

auto event_base::resume_all_awaiter(awaiter_ptr waiter) noexcept -> void // 唤醒所有 suspend awaiter
{                                                                        // 以循环的形式遍历链表
    while (waiter != nullptr)
    {
        auto cur = static_cast<awaiter_base*>(waiter);

        // 将该 awaiter 绑定的协程投放到其绑定的 context 的任务队列里
        cur->m_ctx.submit_task(cur->m_await_coro);  // 重新提交任务
        waiter = cur->m_next;
    }
}

auto event_base::register_awaiter(awaiter_base* waiter) noexcept -> bool // 挂载 suspend awaiter
{
    const auto  set_state = this;
    awaiter_ptr old_value = nullptr;
    // 利用 cas 操作确保挂载操作的原子性

    do
    {
        old_value = m_state.load(std::memory_order_acquire);
        if (old_value == this)
        {
            waiter->m_next = nullptr;
            return false;
        }
        waiter->m_next = static_cast<awaiter_base*>(old_value);

    } while (!m_state.compare_exchange_weak(old_value, waiter, std::memory_order_acquire));

    // 把 watier 头插法，m_state记录着所有的awaitable
    return true;
}

}; // namespace detail

}; // namespace coro