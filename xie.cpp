#include <coroutine>
#include <iostream>
#include <cstdlib>

struct Generator {
    struct promise_type {
        int current_value;
        
        // 初始挂起：创建后立即挂起
        std::suspend_always initial_suspend() { return {}; }
        
        // 最终挂起：协程结束时挂起（避免自动销毁）
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void unhandled_exception() { std::terminate(); }
        
        Generator get_return_object() {
            return Generator{
                std::coroutine_handle<promise_type>::from_promise(*this)
            };
        }
        
        // co_yield 处理
        std::suspend_always yield_value(int value) {
            current_value = value;
            return {};
        }
        
        void return_void() {}
    };

    std::coroutine_handle<promise_type> handle;

    explicit Generator(std::coroutine_handle<promise_type> h) : handle(h) {}
    
    // 禁止拷贝
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    
    // 允许移动
    Generator(Generator&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }
    
    ~Generator() {
        if (handle) handle.destroy();
    }
    
    bool next() {
        if (!handle || handle.done()) return false;
        handle.resume();
        return !handle.done();
    }
    
    int value() const {
        if (!handle) return -1;
        return handle.promise().current_value;
    }
};

// 协程函数：生成指定范围的整数
Generator sequence(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

int main() {
    auto seq = sequence(10, 20);
    
    // 获取序列值
    while (seq.next()) {
        std::cout << seq.value() << " " << std::endl;
    }
    
    return 0;
}



Generator 结构体（协程返回对象）
cpp
struct Generator {
    struct promise_type { ... }; // 协程承诺类型
    std::coroutine_handle<promise_type> handle; // 协程句柄
    // 构造函数/析构函数/移动操作
    bool next(); // 恢复协程执行
    int value() const; // 获取当前值
};
核心作用：作为协程的返回对象，控制协程执行和获取数据

关键组件：

promise_type：协程内部状态管理

coroutine_handle：协程生命周期控制器

2. promise_type（协程承诺类型）
cpp
struct promise_type {
    int current_value; // 存储yield的值
    
    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    
    void unhandled_exception() { std::terminate(); }
    
    Generator get_return_object() {
        return Generator{
            std::coroutine_handle<promise_type>::from_promise(*this)
        };
    }
    
    std::suspend_always yield_value(int value) {
        current_value = value;
        return {};
    }
    
    void return_void() {}
};
关键方法：

initial_suspend(): 返回suspend_always，协程创建后立即挂起（延迟执行）

final_suspend(): 协程结束时挂起（防止自动销毁）

yield_value(): 处理co_yield，存储值并挂起协程

get_return_object(): 创建关联的Generator对象

return_void(): 处理协程无返回值的情况

3. Generator 方法实现
cpp
// 移动构造函数（禁止拷贝）
Generator(Generator&& other) noexcept : handle(other.handle) {
    other.handle = nullptr;
}

// 析构函数
~Generator() {
    if (handle) handle.destroy();
}

// 恢复协程执行
bool next() {
    if (!handle || handle.done()) return false;
    handle.resume();
    return !handle.done();
}

// 获取当前值
int value() const {
    return handle.promise().current_value;
}
生命周期管理：

移动语义转移所有权，避免重复销毁

析构时销毁协程帧

执行控制：

next() 恢复协程执行，返回是否还有值

value() 获取promise中存储的值

4. 协程函数 sequence()
cpp
Generator sequence(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i; // 挂起并返回值
    }
    // 隐含 co_return;
}
行为：

生成从start到end-1的整数序列

每次循环使用co_yield挂起并返回当前值

循环结束自动调用co_return

5. main 函数
cpp
int main() {
    auto seq = sequence(10, 20); // 创建协程（挂起状态）
    
    while (seq.next()) { // 恢复协程执行
        std::cout << seq.value() << " "; // 获取当前值
    }
    // 输出: 10 11 12 ... 19
}
执行流程：

创建协程时立即挂起（initial_suspend）

每次next()调用：

恢复协程执行（resume()）

执行到下一个co_yield后挂起

返回true表示还有值

协程结束后next()返回false

value()始终返回最近yield的值
