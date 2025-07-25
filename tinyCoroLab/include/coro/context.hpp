/**
 * @file context.hpp
 * @author JiahuiWang
 * @brief lab2b
 * @version 1.1
 * @date 2025-03-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "config.h"
#include "coro/engine.hpp"
#include "coro/meta_info.hpp"
#include "coro/task.hpp"

namespace coro
{
/**
 * @brief Welcome to tinycoro lab2b, in this part you will use engine to build context. Like
 * equipping soldiers with weapons, context will use engine to execute io task and computation task.
 *
 * @warning You should carefully consider whether each implementation should be thread-safe.
 *
 * You should follow the rules below in this part:
 *
 * @note Do not modify existing functions and class declaration, which may cause the test to not run
 * correctly, but you can change the implementation logic if you need.
 *
 * @note The location marked by todo is where you must add code, but you can also add code anywhere
 * you want, such as function and class definitions, even member variables.
 */

using config::ctx_id;
using std::atomic;
using std::jthread;
using std::make_unique;
using std::memory_order_acq_rel;
using std::memory_order_acquire;
using std::memory_order_relaxed;
using std::stop_token;
using std::unique_ptr;

using detail::ginfo;
using detail::linfo;

using engine = detail::engine;

class scheduler;

/**
 * @brief Each context own one engine, it's the core part of tinycoro,
 * which can process computation task and io task
 *
 */
class context
{
    using stop_cb = std::function<void()>;

public:
    context() noexcept;
    ~context() noexcept                = default;
    context(const context&)            = delete;
    context(context&&)                 = delete;
    context& operator=(const context&) = delete;
    context& operator=(context&&)      = delete;

    [[CORO_TEST_USED(lab2b)]] auto init() noexcept -> void;

    [[CORO_TEST_USED(lab2b)]] auto deinit() noexcept -> void;

    /**
     * @brief work thread start running
     *
     */
    [[CORO_TEST_USED(lab2b)]] auto start() noexcept -> void;

    /**
     * @brief send stop signal to work thread
     *
     */
    [[CORO_TEST_USED(lab2b)]] auto notify_stop() noexcept -> void;

    /**
     * @brief wait work thread stop
     *
     */
    inline auto join() noexcept -> void { m_job->join(); }

    inline auto submit_task(task<void>&& task) noexcept -> void
    {
        // 传递右值，意味外部失去所有权，所以 task.detach();
        auto handle = task.handle();
        task.detach();
        this->submit_task(handle);
    }

    inline auto submit_task(task<void>& task) noexcept -> void { submit_task(task.handle()); }

    /**
     * @brief submit one task handle to context
     * 外部调用
     * @param handle
     */
    [[CORO_TEST_USED(lab2b)]] auto submit_task(std::coroutine_handle<> handle) noexcept -> void;

    /**
     * @brief get context unique id
     *
     * @return ctx_id
     */
    inline auto get_ctx_id() noexcept -> ctx_id { return m_id; }

    /**
     * @brief add reference count of context
     *
     * @param register_cnt
     */
    [[CORO_TEST_USED(lab2b)]] auto register_wait(int register_cnt = 1) noexcept -> void;

    /**
     * @brief reduce reference count of context
     *
     * @param register_cnt
     */
    [[CORO_TEST_USED(lab2b)]] auto unregister_wait(int register_cnt = 1) noexcept -> void;

    inline auto get_engine() noexcept -> engine& { return m_engine; }

    /**
     * @brief main logic of work thread
     *
     * @param token
     */
    [[CORO_TEST_USED(lab2b)]] auto run(stop_token token) noexcept -> void;

    // TODO[lab2b]: Add more function if you need
    auto set_stop_cb(stop_cb cb) noexcept -> void { m_stop_cb = cb; }

    // 驱动 engine 从任务队列取出任务并执行
    auto process_work() noexcept -> void;

    // 驱动 engine 执行 IO 任务
    auto poll_work() noexcept -> void { m_engine.poll_submit(); }

    // 判断是否没有 IO 任务以及引用计数是否为 0
    auto empty_wait_task() noexcept -> bool
    {
        return m_num_wait_task.load(memory_order_acquire) == 0 && m_engine.empty_io();
    }

private:
    CORO_ALIGN engine   m_engine;
    unique_ptr<jthread> m_job;
    ctx_id              m_id;

    // TODO[lab2b]: Add more member variables if you need
    atomic<size_t> m_num_wait_task{0};
    stop_cb        m_stop_cb; // context 完成所有任务后应该执行的停止逻辑
};

inline context& local_context() noexcept
{
    return *linfo.ctx;
}

inline void submit_to_context(task<void>&& task) noexcept
{
    local_context().submit_task(std::move(task));
}

inline void submit_to_context(task<void>& task) noexcept
{
    local_context().submit_task(task.handle());
}

inline void submit_to_context(std::coroutine_handle<> handle) noexcept
{
    local_context().submit_task(handle);
}

}; // namespace coro
