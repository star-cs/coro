#include "coro/engine.hpp"
#include "coro/io/io_info.hpp"
#include "coro/task.hpp"

namespace coro::detail
{
using std::memory_order_relaxed;

auto engine::init() noexcept -> void
{
    // TODO[lab2a]: Add you codes
}

auto engine::deinit() noexcept -> void
{
    // TODO[lab2a]: Add you codes
}

auto engine::ready() noexcept -> bool
{
    // TODO[lab2a]: Add you codes
    return {};
}

auto engine::get_free_urs() noexcept -> ursptr
{
    // TODO[lab2a]: Add you codes
    return {};
}

auto engine::num_task_schedule() noexcept -> size_t
{
    // TODO[lab2a]: Add you codes
    return {};
}

auto engine::schedule() noexcept -> coroutine_handle<>
{
    // TODO[lab2a]: Add you codes
    return {};
}

auto engine::submit_task(coroutine_handle<> handle) noexcept -> void
{
    // TODO[lab2a]: Add you codes
}

auto engine::exec_one_task() noexcept -> void
{
    auto coro = schedule();
    coro.resume();
    if (coro.done())
    {
        clean(coro);
    }
}

auto engine::handle_cqe_entry(urcptr cqe) noexcept -> void
{
    auto data = reinterpret_cast<io::detail::io_info*>(io_uring_cqe_get_data(cqe));
    data->cb(data, cqe->res);
}

auto engine::poll_submit() noexcept -> void
{
    // TODO[lab2a]: Add you codes
}

auto engine::add_io_submit() noexcept -> void
{
    // TODO[lab2a]: Add you codes
}

auto engine::empty_io() noexcept -> bool
{
    // TODO[lab2a]: Add you codes
    return {};
}
}; // namespace coro::detail
