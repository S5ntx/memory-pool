#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include <cstring>
#include <random>
#include <algorithm>
#include <atomic>

using namespace Kama_memoryPool;

// 基础分配测试
void testBasicAllocation() 
{
    std::cout << "Running basic allocation test..." << std::endl;
    
    // 测试小内存分配
    void* ptr1 = MemoryPool::allocate(8);
    assert(ptr1 != nullptr);
    MemoryPool::deallocate(ptr1, 8);

    // 测试中等大小内存分配
    void* ptr2 = MemoryPool::allocate(1024);
    assert(ptr2 != nullptr);
    MemoryPool::deallocate(ptr2, 1024);

    // 测试大内存分配（超过MAX_BYTES）
    void* ptr3 = MemoryPool::allocate(1024 * 1024);
    assert(ptr3 != nullptr);
    MemoryPool::deallocate(ptr3, 1024 * 1024);

    std::cout << "Basic allocation test passed!" << std::endl;
}

// 内存写入测试
void testMemoryWriting() 
{
    std::cout << "Running memory writing test..." << std::endl;

    // 分配并写入数据
    const size_t size = 128;
    char* ptr = static_cast<char*>(MemoryPool::allocate(size));
    assert(ptr != nullptr);

    // 写入数据
    for (size_t i = 0; i < size; ++i) 
    {
        ptr[i] = static_cast<char>(i % 256);
    }

    // 验证数据
    for (size_t i = 0; i < size; ++i) 
    {
        assert(ptr[i] == static_cast<char>(i % 256));
    }

    MemoryPool::deallocate(ptr, size);
    std::cout << "Memory writing test passed!" << std::endl;
}

// 多线程测试
void testMultiThreading() 
{
    std::cout << "Running multi-threading test..." << std::endl;

    const int NUM_THREADS = 4;
    const int ALLOCS_PER_THREAD = 1000;
    std::atomic<bool> has_error{false};// 定义了一个原子布尔变量 has_error，初始值为 false。这个变量用于在线程之间安全地共享错误状态，防止数据竞争
    /*
       //  使用 auto 自动推断变量类型  threadFunc 是一个 lambda 表达式
           [&has_error] 表示捕获外部变量 has_error 的引用，意味着 threadFunc 可以修改 has_error

       //  Lambda 表达式是一种在 C++ 中定义匿名函数（或称为内联函数）的方式。它允许你在代码中快速定义和使用一个函数，而不需要提前声明一个独立的函数。
           语法形式通常如下： [捕获列表] (参数列表) -> 返回类型 { 函数体 }
           捕获列表（Capture List）：定义如何访问外部作用域的变量。可以通过值捕获（[x]）或引用捕获（[&x]）等方式捕获外部变量。
           参数列表（Parameter List）：定义函数的参数，与普通函数一样。
           返回类型（Return Type）：定义返回值类型，通常可以省略，编译器会推导。
           函数体（Body）：定义函数内部的操作。

    */
    auto threadFunc = [&has_error]()  
    { 
        try 
        {
            std::vector<std::pair<void*, size_t>> allocations;
            allocations.reserve(ALLOCS_PER_THREAD);                     // 预留足够的空间来存储每个线程的 1000 次分配（提高效率）
            
            for (int i = 0; i < ALLOCS_PER_THREAD && !has_error; ++i) 
            {
                size_t size = (rand() % 256 + 1) * 8;                   // 生成一个 8 到 2048 字节之间的随机内存分配大小
                void* ptr = MemoryPool::allocate(size);
                
                if (!ptr) 
                {
                    std::cerr << "Allocation failed for size: " << size << std::endl;
                    has_error = true;
                    break;
                }
                
                allocations.push_back({ptr, size});
                
                if (rand() % 2 && !allocations.empty())                 // 如果 allocations 不为空且 rand() % 2 为真，则执行内存释放操作
                {
                    size_t index = rand() % allocations.size();         // 随机选择一个分配的内存块
                    MemoryPool::deallocate(allocations[index].first, allocations[index].second);     // 释放随机选择的内存块
                    allocations.erase(allocations.begin() + index);     // 从 allocations 中移除已经释放的内存
                }
            }
            
            for (const auto& alloc : allocations)                       // 遍历 allocations 中的每个元素
            {
                MemoryPool::deallocate(alloc.first, alloc.second);      // 释放每个分配的内存
            }
        }
        catch (const std::exception& e)                                 // 捕获可能抛出的异常，并输出异常信息
        {
            std::cerr << "Thread exception: " << e.what() << std::endl;
            /*
               // std::exception 是 C++ 标准库中定义的一个基类，用于表示所有标准异常的基类。
                  std::exception 类提供了一个成员函数 what()，可以返回描述异常的字符串
                  what() 函数： what() 是一个成员函数，用于获取异常的具体描述信息。它返回一个 const char* 类型的 C 风格字符串，表示异常的原因或描述。
                  what() 函数常常用来捕获异常时，输出异常信息。
                  因为 std::exception 是所有标准异常的基类，所以我们通常会捕获它的派生类（如 std::runtime_error、std::out_of_range 等）。通过调用 what()，我们可以获得异常的具体描述。
            */
            has_error = true;
            /*
               // std::cerr 标准错误输出流，用于输出错误信息，通常用于输出错误消息、警告信息等。
                  std::cerr 和 std::cout 的主要区别在于：
                  std::cerr 通常不进行缓冲，它的输出会立即显示，不会等待缓冲区满。
                  std::cout 会进行缓冲，输出会稍微延迟直到缓冲区满或遇到换行符。
            */
        }
    };

    std::vector<std::thread> threads;                                   // 定义一个 std::vector 存储线程对象
    for (int i = 0; i < NUM_THREADS; ++i) 
    {
        threads.emplace_back(threadFunc);                               // 为每个线程创建并启动一个 threadFunc 执行的线程
    }

    for (auto& thread : threads) 
    {
        thread.join();                                                 // 等待每个线程执行完毕
    }

    std::cout << "Multi-threading test passed!" << std::endl;
}

// 边界测试
void testEdgeCases() 
{
    std::cout << "Running edge cases test..." << std::endl;
    
    // 测试0大小分配
    void* ptr1 = MemoryPool::allocate(0);
    assert(ptr1 != nullptr);
    MemoryPool::deallocate(ptr1, 0);
    
    // 测试最小对齐大小
    void* ptr2 = MemoryPool::allocate(1);
    assert(ptr2 != nullptr);
    assert((reinterpret_cast<uintptr_t>(ptr2) & (ALIGNMENT - 1)) == 0);
    MemoryPool::deallocate(ptr2, 1);
    
    // 测试最大大小边界
    void* ptr3 = MemoryPool::allocate(MAX_BYTES);
    assert(ptr3 != nullptr);
    MemoryPool::deallocate(ptr3, MAX_BYTES);
    
    // 测试超过最大大小
    void* ptr4 = MemoryPool::allocate(MAX_BYTES + 1);
    assert(ptr4 != nullptr);
    MemoryPool::deallocate(ptr4, MAX_BYTES + 1);
    
    std::cout << "Edge cases test passed!" << std::endl;
}

// 压力测试
void testStress() 
{
    std::cout << "Running stress test..." << std::endl;

    const int NUM_ITERATIONS = 10000;
    std::vector<std::pair<void*, size_t>> allocations;                    // 每个 pair 包含一个内存块的指针 void* 和该内存块的大小 size_t
    allocations.reserve(NUM_ITERATIONS);
    /*
        // 预留足够的空间来存储 10000 次分配的内存地址和大小，这样做可以提高效率，避免 std::vector 在执行过程中多次重新分配内存。
          reserve 是 C++ 标准库 std::vector 类中的一个成员函数。
          它的作用是预先为 std::vector 分配一定数量的内存，以避免在后续操作中频繁重新分配内存，进而提高性能
    */

    for (int i = 0; i < NUM_ITERATIONS; ++i) 
    {
        size_t size = (rand() % 1024 + 1) * 8;
        void* ptr = MemoryPool::allocate(size);
        assert(ptr != nullptr);
        allocations.push_back({ptr, size});
    }

    // 随机顺序释放
    std::random_device rd;                                                // 用来获取硬件生成的随机数（如果平台支持的话），它提供一种不可预测的种子
    std::mt19937 g(rd());   // 使用 rd 提供的随机种子初始化 std::mt19937 随机数引擎（Mersenne Twister 算法）。这是一个高效的伪随机数生成器
    std::shuffle(allocations.begin(), allocations.end(), g);              // 使用 std::shuffle 函数将 allocations 向量中的元素顺序打乱（即随机排序）
    for (const auto& alloc : allocations) 
    {
        MemoryPool::deallocate(alloc.first, alloc.second);
    }

    std::cout << "Stress test passed!" << std::endl;
}

int main() 
{
    try 
    {
        std::cout << "Starting memory pool tests..." << std::endl;

        testBasicAllocation();
        testMemoryWriting();
        testMultiThreading();
        testEdgeCases();
        testStress();

        std::cout << "All tests passed successfully!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}