#include <coroutine>
#include <iostream>
#include <optional>
#include <string>

// 面向用户的对象
class Task
{
public:
    class promise_type; // 满足该定义后便可以作为协程函数的返回值，注意协程函数必须以满足该定义的类型作为返回值。
    using handle_type = std::coroutine_handle<promise_type>; // 协程句柄类型
    // 1.  将 promise_type 的定义放到 UserFacing 里
    class promise_type // 与 Task 关联的 promise 对象
    {
        friend class Task;

    public:
        //
        Task get_return_object() // 用于构造协程（编译器生成调用代码）
        {
            auto handle = handle_type::from_promise(*this);
            return Task(handle);
        }

        // 控制协程创建完成后的调度逻辑（编译器生成调用代码）
        /**
        Q: 为何样例程序的 initial_suspend 函数前要添加 constexpr？
        A: std::suspend_always 和 std::suspend_never 都是非常简单且含义十分固定明确的 awaiter，
        所以成员函数均有 constexpr 限制，如果 initial_suspend 函数直接返回二者其一的话也可以添加 constexpr
        方便编译器做优化，其他函数也同理。
        */
        constexpr std::suspend_always initial_suspend() { return {}; }

        // 协程返回值时会调用该函数（编译器生成调用代码）
        // 用户可以自定义 return_value 函数的参数类型
        void return_value(std::string result) { m_result = result; }

        // 协程 yeild 值时会调用该函数（编译器生成调用代码）
        /**
        yield_value 通过返回 awaiter 类型来决定协程的执行权如何处理，一般返回 std::suspend_alaways
        转移控制权到调用者，用户也可返回自定义的 awaiter，但通常不要返回 std::suspend_never 等让协程继续运行的
        awaiter，因为此时协程继续运行的话如果再次碰到 co_yield 那么上次 yield 的值就会被覆盖。
        */
        std::suspend_always yield_value(int value)
        {
            m_yield = value;
            return {};
        }

        // 协程运行抛出异常时会调用该函数（编译器生成调用代码）
        void unhandled_exception() { m_exception = std::current_exception(); }

        // 协程执行完毕后会调用该函数（编译器生成调用代码）
        /**
        与 initial_suspend 类似，final_suspend 函数负责协程执行结束后的调度点逻辑，返回值同样是 awaiter
        类型，用户可以通过自定义 awaiter 来转移执行权，也可以直接返回 std::suspend_alaways 或者 std::suspend_never，调用
        final_suspend 函数时会执行下列伪代码：

        如果 final_suspend 返回了 suspend_never，那么编译器会接着执行后续的资源清理操作，如果 UserFacing
        在析构函数中再次执行 handle.destroy，那么会出现 core dump，所以一般建议不要返回
        suspend_never，因为资源的释放最好在用户侧来做。
        */
        constexpr std::suspend_always final_suspend() noexcept { return {}; }

    private:
        std::exception_ptr m_exception{nullptr};
        /**
        使用 std::optional 来存储 co_yield 的值便于协程调用方判断迭代循环是否结束，当 value 是 std::nullopt
        时，则表明协程 co_yield 部分结束。

        std::optional<T>

        - 包装一个可能存在的值。它要么包含一个类型为 T 的值，要么不包含任何值（空状态）。

        std::nullopt
        - 表示空 optional 的常量，用于显式初始化或重置 std::optional 为空状态。
        */
        std::optional<std::string> m_result{std::nullopt};
        std::optional<int>         m_yield{std::nullopt};
    };

public:
    // 禁止拷贝构造
    Task(const Task&) = delete;

    // 允许移动构造
    Task(Task&& other) : m_handle(other.m_handle) { other.m_handle = nullptr; }

    // 禁止拷贝赋值
    Task& operator=(const Task& other) = delete;

    // 允许移动赋值
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

    // 析构自动释放关联的协程句柄
    /* final_suspend返回std::suspend_always，则析构调用 handle.destroy*/
    ~Task() { m_handle.destroy(); }

public:
    void resume() { m_handle.resume(); }

    // 用户侧不断调用 next 驱动协程 yield 值
    std::optional<int> next()
    {
        promise_type& p = m_handle.promise(); // 首先获取协程句柄关联的 promise
        p.m_yield       = std::nullopt;       // 必须先置存储 yield 的变量为 nullopt
                                              // 这样才能正确使得 yield 结束后 next 返回 nullopt
        p.m_exception = nullptr;
        if (!m_handle.done())
        {
            m_handle.resume(); // 恢复协程运行
        }
        // 再次抛出异常
        if (p.m_exception)
        {
            std::rethrow_exception(p.m_exception);
        }
        return p.m_yield;
    }

    // 用户手动调用该函数获取协程返回值，注意协程可能此时并没有返回值
    std::optional<std::string> result() { return m_handle.promise().m_result; }

private:
    Task(handle_type handle) : m_handle(handle) {}

private:
    handle_type m_handle;
};

/**
run 函数因为在函数体包含了协程关键字，所以被当作协程处理，其返回值也较为特殊
*/
Task run(int i)
{
    std::cout << "task " << i << " start" << std::endl;
    co_yield 1;
    for (int i = 2; i <= 5; i++)
    {
        co_yield i;
    }
    std::cout << "task " << i << " end" << std::endl;
    co_return "task run finish";
}

int main(int argc, char const* argv[])
{
    /* code */
    auto               task = run(5);
    std::optional<int> val;
    while ((val = task.next()))
    {
        std::cout << "get yield value: " << (*val) << std::endl;
    }
    std::cout << "get return value: " << (*task.result()) << std::endl;
    return 0;
}