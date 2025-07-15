#include "coro/context.hpp"
#include "coro/scheduler.hpp"

namespace coro
{
context::context() noexcept
{
    m_id = ginfo.context_id.fetch_add(1, std::memory_order_relaxed);
}

auto context::init() noexcept -> void
{
    // TODO[lab2b]: Add you codes
}

auto context::deinit() noexcept -> void
{
    // TODO[lab2b]: Add you codes
}

auto context::start() noexcept -> void
{
    // TODO[lab2b]: Add you codes
    m_job = make_unique<jthread>(
        [this](stop_token token)
        {
            this->init();
            this->run(token);
            this->deinit();
        });
}

auto context::notify_stop() noexcept -> void
{
    // TODO[lab2b]: Add you codes
}

auto context::submit_task(std::coroutine_handle<> handle) noexcept -> void
{
    // TODO[lab2b]: Add you codes
}

auto context::register_wait(int register_cnt) noexcept -> void
{
    // TODO[lab2b]: Add you codes
}

auto context::unregister_wait(int register_cnt) noexcept -> void
{
    // TODO[lab2b]: Add you codes
}

auto context::run(stop_token token) noexcept -> void
{
    // TODO[lab2b]: Add you codes
}

}; // namespace coro