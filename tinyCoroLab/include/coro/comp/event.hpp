/**
 * @file event.hpp
 * @author JiahuiWang
 * @brief lab4a
 * @version 1.1
 * @date 2025-03-24
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <atomic>
#include <coroutine>

#include "coro/attribute.hpp"
#include "coro/concepts/awaitable.hpp"
#include "coro/context.hpp"
#include "coro/detail/container.hpp"
#include "coro/detail/types.hpp"

namespace coro
{
/**
 * @brief Welcome to tinycoro lab4a, in this part you will build the basic coroutine
 * synchronization component - event by modifing event.hpp and event.cpp. Please ensure
 * you have read the document of lab4a.
 *
 * @warning You should carefully consider whether each implementation should be thread-safe.
 *
 * You should follow the rules below in this part:
 *
 * @note The location marked by todo is where you must add code, but you can also add code anywhere
 * you want, such as function and class definitions, even member variables.
 *
 * @note lab4 and lab5 are free designed lab, leave the interfaces that the test case will use,
 * and then, enjoy yourself!
 */
class context;

namespace detail
{
// TODO[lab4a]: Add code that you don't want to use externally in namespace detail

class event_base
{
public:
    struct awaiter_base
    {
        awaiter_base(context& ctx, event_base& e) noexcept : m_ctx(ctx), m_ev(e) {}

        inline auto next() noexcept -> awaiter_base* { return m_next; }

        auto await_ready() noexcept -> bool;

        auto await_suspend(std::coroutine_handle<> handle) noexcept -> bool;

        auto await_resume() noexcept -> void;

        context&                m_ctx;                 // 绑定的 context
        event_base&             m_ev;                  // 绑定的 event
        awaiter_base*           m_next{nullptr};       // 链表的 next 指针
        std::coroutine_handle<> m_await_coro{nullptr}; // 待 resume 的协程句柄
    };

    event_base(bool initial_set = false) noexcept : m_state((initial_set) ? this : nullptr) {}

    ~event_base() noexcept                   = default;
    event_base(const event_base&)            = delete;
    event_base(event_base&&)                 = delete;
    event_base& operator=(const event_base&) = delete;
    event_base& operator=(event_base&&)      = delete;

    // 判断 event 是否被 set，根据 m_state 是否等于 this
    inline auto is_set() const noexcept -> bool { return m_state.load(std::memory_order_acquire) == this; }

    auto set_state() noexcept -> void; // 设置 event，唤醒所有 suspend awaiter

    auto resume_all_awaiter(awaiter_ptr waiter) noexcept -> void; // 唤醒所有 suspend awaiter

    auto register_awaiter(awaiter_base* waiter) noexcept -> bool; // 挂载 suspend awaiter

private:
    // 链表头
    std::atomic<awaiter_ptr> m_state{nullptr};
};

}; // namespace detail

// TODO[lab4a]: This event is an example to make complie success,
// You should delete it and add your implementation, I don't care what you do,
// but keep the function set() and wait()'s declaration same with example.
template<typename return_type = void>
class event : public detail::event_base, public detail::container<return_type>
{
public:
    // Just make compile success
    struct [[CORO_AWAIT_HINT]] awaiter : public detail::event_base::awaiter_base
    {
        using awaiter_base::awaiter_base;
        auto await_resume() noexcept -> decltype(auto) // 重新定义基类的同名函数
        {
            detail::event_base::awaiter_base::await_resume();
            return static_cast<event&>(m_ev).result();  // 获取 detail::container<return_type> 里的 result
        }
    };

    [[CORO_AWAIT_HINT]] awaiter wait() noexcept { return awaiter(local_context(), *this); }

    template<typename value_type>
    auto set(value_type&& value) noexcept -> void
    {
        this->return_value(std::forward<value_type>(value));
        set_state();
    }
};

template<>
class event<void> : public detail::event_base
{
public:
    struct [[CORO_AWAIT_HINT]] awaiter : public detail::event_base::awaiter_base
    {
        // 将基类 awaiter_base 的构造函数引入派生类 awaiter
        // 的作用域，使得派生类可以直接复用基类的构造函数，无需重新定义。
        using awaiter_base::awaiter_base;
    };

    [[CORO_AWAIT_HINT]] awaiter wait() noexcept { return awaiter(local_context(), *this); }

    auto set() noexcept -> void { set_state(); }
};

/**
 * @brief RAII for event
 *
 */
class event_guard
{
    using guard_type = event<>;

public:
    event_guard(guard_type& ev) noexcept : m_ev(ev) {}
    ~event_guard() noexcept { m_ev.set(); }

private:
    guard_type& m_ev;
};

}; // namespace coro
