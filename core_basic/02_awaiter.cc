#include <coroutine>
#include <iostream>

std::coroutine_handle<> global_handle;

class Event
{
public:
    Event() { std::cout << "event construct" << std::endl; }
    ~Event() { std::cout << "event deconstruct" << std::endl; }

    bool await_ready() { return false; }
    // 保存了协程句柄，则可以延长协程帧的生命周期
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> caller)
    {
        return caller;
        // return global_handle;
    }
    void await_resume() { std::cout << "await resume called" << std::endl; }
};

class Task
{
public:
    class promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    class promise_type
    {
        friend class Task;

    public:
        promise_type(int id) : m_id(id) {}
        Task get_return_object()
        {
            auto handle   = handle_type::from_promise(*this);
            global_handle = handle;
            return Task(handle, m_id);
        }

        constexpr std::suspend_never initial_suspend() { return {}; }

        void return_void() {}

        void unhandled_exception() {}

        constexpr std::suspend_always final_suspend() noexcept { return {}; }

    private:
        int m_id;
    };

public:
    Task(const Task&) = delete;
    Task(Task&& other) : m_handle(other.m_handle) { other.m_handle = nullptr; }
    Task& operator=(const Task& other) = delete;
    Task& operator=(Task&& other)
    {
        if (m_handle)
        {
            m_handle.destroy();
        }
        m_handle       = other.m_handle;
        other.m_handle = nullptr;
        return *this;
    }

    // 对象析构没有调用 句柄的 destroy，所以 run(1) 的 句柄通过 global_handle 延长声明周期
    ~Task() { std::cout << "deconstruct task " << m_id << std::endl; }

public:
    auto operator co_await() { return Event{}; }

    void resume() { m_handle.resume(); }

private:
    Task(handle_type handle, int id) : m_handle(handle), m_id(id)
    {
        std::cout << "construct task " << m_id << std::endl;
    }

private:
    int         m_id;
    handle_type m_handle;
};

/**
1 构造 run(0) 的 promise_type（ID=0）
2 调用 promise.get_return_object()：
    创建协程句柄 handle0
    global_handle = handle0（全局保存）
    打印 construct task 0
3 initial_suspend() 返回 suspend_never → 立即执行协程体
4 打印 task 0 start
5 进入 i==0 分支：co_await run(1)
*/
Task run(int i)
{
    std::cout << "task " << i << " start" << std::endl;
    if (i == 0)
    {
        co_await run(i + 1);
        // 先执行子协程的调用逻辑，当子协程第一次 suspend 时，awaiter 才会被构造

        /**
        调用 Task::operator co_await() → 生成临时 Event 对象：
            打印 event construct

        处理 co_await Event{}：
            await_ready() = false → 需要挂起
            await_suspend(caller_handle)：
                接收 run(0) 的句柄 handle0
                返回 handle0（调用者自身句柄）

        特殊行为（关键）：
            返回自身句柄 → 协程机制立即恢复该句柄（handle0）
            效果：run(0) 未真正挂起，直接继续执行

        销毁临时 Event 对象 → 打印 event deconstruct

        继续执行 run(0)：
            打印 task 0 will suspend
            执行 co_await std::suspend_always{}：
                await_ready() = false
                await_suspend() 返回 void → 挂起协程，返回 main()
        */
    }
    else
    {
        std::cout << "task " << i << " will suspend" << std::endl;
        co_await std::suspend_always{};
    }

    std::cout << "task " << i << " will suspend" << std::endl;
    co_await std::suspend_always{};

    std::cout << "task " << i << " end" << std::endl;
    co_return;
}

int main(int argc, char const* argv[])
{
    /* code */
    auto task = run(0);
    std::cout << "back to main" << std::endl;
    global_handle.resume(); // global_handle=handle1 (run(1))
    /**
    打印 back to main
    global_handle.resume()：
        此时 global_handle 指向 run(1) 的句柄（在 run(1) 启动时被覆盖）
        恢复 run(1) 从上次挂起点继续执行

    run(1) 恢复后：
        打印 task 1 will suspend
        遇到第二个 co_await std::suspend_always{} → 再次挂起

    主函数继续：
        打印 run finish
        main() 结束
    */
    std::cout << "run finish" << std::endl;
    return 0;
}