#include "coro/scheduler.hpp"
#include "coro/context.hpp"
#include <atomic>
#include <memory>

namespace coro
{
auto scheduler::init_impl(size_t ctx_cnt) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    detail::init_meta_info();

    m_ctx_cnt = ctx_cnt;
    m_ctxs    = detail::ctx_container{};
    m_ctxs.reserve(m_ctx_cnt);

    for (int i = 0; i < m_ctx_cnt; i++)
    {
        m_ctxs.emplace_back(std::make_unique<context>());
    }

    m_dispatcher.init(ctx_cnt, &m_ctxs);

    m_ctx_stop_flag = stop_flag_type(m_ctx_cnt, detail::atomic_ref_wrapper<int>{.val = 1});
    m_stop_token    = m_ctx_cnt;

#ifdef ENABLE_MEMORY_ALLOC
    coro::allocator::memory::mem_alloc_config config;
    m_mem_alloc.init(config);
    ginfo.mem_alloc = &m_mem_alloc;
#endif
}

auto scheduler::start_impl() noexcept -> void
{
    for (int i = 0; i < m_ctx_cnt; i++)
    {
        // 当上下文完成所有任务，需要更新 调度器里 的 m_ctx_stop_flag 对应标记。
        m_ctxs[i]->set_stop_cb(
            [&, i]()
            {
                auto cnt = std::atomic_ref(this->m_ctx_stop_flag[i].val).fetch_and(0, memory_order_acq_rel);
                if (this->m_stop_token.fetch_sub(cnt) == cnt)
                {
                    // Stop token decrease to zero, it's time to close all context
                    this->stop_impl();
                }
            });
        m_ctxs[i]->start();
    }
}

auto scheduler::loop_impl() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    start_impl();
    for (int i = 0; i < m_ctx_cnt; i++)
    {
        m_ctxs[i]->join();
    }
}

auto scheduler::stop_impl() noexcept -> void
{
    // TODO[lab2b]: example function
    // This is an example which just notify stop signal to each context,
    // if you don't need this, function just ignore or delete it
    for (int i = 0; i < m_ctx_cnt; i++)
    {
        m_ctxs[i]->notify_stop();
    }
}

auto scheduler::submit_task_impl(std::coroutine_handle<> handle) noexcept -> void
{
    // TODO[lab2b]: Add you codes
    // 不要在 scheduler::loop 结束后再添加新任务
    assert(this->m_stop_token.load(std::memory_order_acquire) != 0 && "error! submit task after scheduler loop finish");

    size_t ctx_id = m_dispatcher.dispatch();
    // 直接增加引用计数是不合理的，
    // 根据 context 的运行状态来增加引用计数，避免冗余增加
    /*
    1. 对 m_ctx_stop_flag[ctx_id].val 的值执行 ​原子位或（OR）操作，将最低位设为 1（若原本为
    0 则标记为活跃状态）。返回操作前的旧值​（0 表示此前空闲，1 表示已有任务）。
    2.若旧值为 0（此前空闲）→ 1 - 0 = 1，表示需增加引用计数。
    若旧值为 1（已有任务）→ 1 - 1 = 0，表示无需增加计数。
    */
    m_stop_token.fetch_add(
        1 - std::atomic_ref(m_ctx_stop_flag[ctx_id].val).fetch_or(1, memory_order_acq_rel), memory_order_acq_rel);
    m_ctxs[ctx_id]->submit_task(handle);
}
}; // namespace coro
