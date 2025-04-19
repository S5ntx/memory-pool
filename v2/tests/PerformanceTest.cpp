#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <thread>

using namespace Kama_memoryPool;
using namespace std::chrono;                        // 时间处理功能

// 计时器类
class Timer 
{
    high_resolution_clock::time_point start;
    /*
       // high_resolution_clock 是 C++11 中 <chrono> 头文件定义的一个时钟类型，它提供高精度的时间测量。
          high_resolution_clock 是一个时钟类，它的作用是获取当前时间点并测量时间差。high_resolution_clock 通常会有最高的精度，适用于精细的时间度量。
          time_point 是 high_resolution_clock 类中的一个类型，表示一个“时间点”。
          time_point 存储了从某个起始时间点（通常是系统时间的纪元）到当前时间的持续时间。
          start 是 high_resolution_clock::time_point 类型的变量，用来存储计时器开始时的时间点。这里的 start 是一个时间点，表示计时器开始的时刻。


    */
public:
    Timer() : start(high_resolution_clock::now()) {}
    /*
       // 调用 high_resolution_clock 类的静态成员函数 now()，返回当前的时间点
       // 构造函数通过初始化列表 : start(high_resolution_clock::now()) 初始化 start，即在 Timer 对象创建时，start 被设置为当前时间
    */

    double elapsed() 
    {
        auto end = high_resolution_clock::now();
        return duration_cast<microseconds>(end - start).count() / 1000.0; // 转换为毫秒
    }
    /*
       // Timer 类的成员函数 elapsed : 用于计算从计时器开始到调用 elapsed() 时的时间间隔，单位为毫秒
          再次调用 high_resolution_clock::now()，获取当前时间点并将其存储在 end 变量中。此时，end 表示调用 elapsed() 时的当前时间
       // end - start 计算的是从 start 时间到 end 时间的差值，这个差值是一个 duration 对象，表示从 start 到 end 之间的时间间隔
          duration_cast 是用来将一个 duration 类型转换为另一种类型的函数。
          在这里，duration_cast<microseconds> 将时间间隔从默认的 duration 类型转换为 microseconds（微秒）
       // elapsed() 函数返回从计时器开始到调用时的时间间隔，单位为毫秒
    */
};

// 性能测试类
class PerformanceTest 
{
private:
    // 测试统计信息
    struct TestStats 
    {
        double memPoolTime{0.0};     // 内存池耗时
        double systemTime{0.0};      // 系统分配耗时
        size_t totalAllocs{0};       // 总分配次数
        size_t totalBytes{0};        // 总分配字节数
    };

public:
    // 1. 系统预热
    static void warmup() 
    {
        std::cout << "Warming up memory systems...\n";
        // 使用 pair 来存储指针和对应的大小
        std::vector<std::pair<void*, size_t>> warmupPtrs;
        
        // 预热内存池
        for (int i = 0; i < 1000; ++i) 
        {
            for (size_t size : {32, 64, 128, 256, 512}) {
                void* p = MemoryPool::allocate(size);
                warmupPtrs.emplace_back(p, size);  // 存储指针和对应的大小
            }
        }
        
        // 释放预热内存
        for (const auto& [ptr, size] : warmupPtrs) 
        {
            MemoryPool::deallocate(ptr, size);  // 使用实际分配的大小进行释放
            //  C++17 的结构化绑定语法（[ptr, size]），
            //  它将 warmupPtrs 向量中的每个元素（即 std::pair<void*, size_t>）解包成 ptr 和 size。这样，你可以直接访问每个分配的内存块指针和大小
        }
        /*
           // 系统预热的目的是什么？ 
            // 避免第一次分配的延迟：在某些内存池实现中，第一次请求内存时可能需要初始化一些资源（例如内存池的管理数据结构）。
              这种初始化可能会导致第一次分配的操作特别慢
            // 触发内存池内部的优化机制：有些内存池在初始化时可能会进行一些优化（比如内存对齐、内存池扩展等）
            // 测试内存池的稳定性：通过进行一定量的内存分配和释放，预热过程可以帮助检测内存池是否存在问题
        */
        std::cout << "Warmup complete.\n\n";
    }

    // 2. 小对象分配测试
    static void testSmallAllocation() 
    {
        constexpr size_t NUM_ALLOCS = 100000;
        constexpr size_t SMALL_SIZE = 32;
        
        std::cout << "\nTesting small allocations (" << NUM_ALLOCS << " allocations of " << SMALL_SIZE << " bytes):" << std::endl;
        
        // 测试内存池
        {
            Timer t;
            std::vector<void*> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
                ptrs.push_back(MemoryPool::allocate(SMALL_SIZE));
                
                // 模拟真实使用：部分立即释放
                if (i % 4 == 0) 
                {
                    MemoryPool::deallocate(ptrs.back(), SMALL_SIZE);
                    ptrs.pop_back();
                }
            }
            
            for (void* ptr : ptrs) 
            {
                MemoryPool::deallocate(ptr, SMALL_SIZE);
            }
            
            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
            /*
               // std::fixed: 这个操作符会强制输出浮点数时以 固定的小数点格式 来显示，而不是科学计数法
               // std::setprecision(3): 这个操作符用于指定浮点数输出的小数点后 精确到多少位。
            */
        }
        
        // 测试new/delete
        {
            Timer t;
            std::vector<void*> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
                ptrs.push_back(new char[SMALL_SIZE]); // 每个 char 占用 1 字节，因此总共占用 32 字节内存
                /*
                   //  new：用于动态分配一个对象，并返回指向该对象的指针。
                       Type* ptr = new Type;
                       为一个 Type 类型的对象在堆上分配内存并返回指向该对象的指针。
                   //  new[]：用于动态分配一个数组，并返回指向该数组第一个元素的指针。
                       Type* ptr = new Type[size];
                       为 size 个 Type 类型的对象在堆上分配内存，并返回指向数组首元素的指针
                   //  delete：用于释放通过 new 分配的单个对象
                   //  delete[]：用于释放通过 new[] 分配的数组。
                */
                
                if (i % 4 == 0) 
                {
                    delete[] static_cast<char*>(ptrs.back());
                    /*
                      // delete[] 用来释放通过 new[] 分配的 数组。需要注意的是，只有使用 new[] 分配的数组才能用 delete[] 来释放。
                        在这里，delete[] 用来释放之前通过 new char[SMALL_SIZE] 分配的内存
                      // delete[] 是 操作符 需要操作的对象是通过 new[] 分配的数组。delete[] 语法要求它后面直接跟着 要释放的数组指针
                    */
                    ptrs.pop_back();
                }
            }
            
            for (void* ptr : ptrs) 
            {
                delete[] static_cast<char*>(ptr);
            }
            
            std::cout << "New/Delete: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
        }
    }
    
    // 3. 多线程测试
    static void testMultiThreaded() 
    {
        constexpr size_t NUM_THREADS = 4;
        constexpr size_t ALLOCS_PER_THREAD = 25000;
        constexpr size_t MAX_SIZE = 256;
        
        std::cout << "\nTesting multi-threaded allocations (" << NUM_THREADS 
                  << " threads, " << ALLOCS_PER_THREAD << " allocations each):" 
                  << std::endl;
        /*
           // 使用了 Lambda 表达式 来定义一个匿名函数（threadFunc）。
              这个 Lambda 表达式的参数是 bool useMemPool，用来决定是否使用内存池（true 表示使用内存池，false 表示使用 new/delete）
           // [capture](parameters) -> return_type { body }
           // 不写捕获列表：如果 Lambda 不需要访问任何外部变量，可以省略 []，写成空的 []
              捕获指定变量：可以通过捕获列表 [x, y] 或 [=] 等指定捕获的外部变量。
                [x, y]：捕获外部变量 x 和 y。
                [=]：按值捕获所有外部变量。Lambda 内部无法修改这些变量
                [&]：按引用捕获所有外部变量。Lambda 内部可以修改这些变量
                [this]：捕获当前对象（在成员函数中常用）。
           // 自动推导返回类型：如果 Lambda 的返回类型可以通过表达式自动推导，那么你可以不写 -> return_type
           // 无参数 Lambda：如果 Lambda 不需要接受任何参数，参数列表可以省略：
        */
        auto threadFunc = [](bool useMemPool) 
        {
            std::random_device rd;     // 用于生成随机种子，通常由硬件提供（依赖于系统）
            std::mt19937 gen(rd());    // 初始化一个 Mersenne Twister 随机数生成器，通过 rd() 来种子化它。
            std::uniform_int_distribution<> dis(8, MAX_SIZE); 
            // 建一个均匀分布的随机数生成器，生成范围在 8 到 MAX_SIZE（即 256）之间的整数。这样可以模拟不同大小的内存块分配
            std::vector<std::pair<void*, size_t>> ptrs;
            ptrs.reserve(ALLOCS_PER_THREAD);
            
            for (size_t i = 0; i < ALLOCS_PER_THREAD; ++i) 
            {
                size_t size = dis(gen);
                void* ptr = useMemPool ? MemoryPool::allocate(size) : new char[size];
                /*
                   // 根据 useMemPool 参数的值，决定使用内存池 MemoryPool::allocate(size) 或者直接使用 new char[size] 来分配内存
                      通过三目运算符（又称条件运算符）来根据 useMemPool 参数的值，决定选择使用内存池分配内存还是直接使用 new 运算符进行分配
                   // condition ? expression_if_true : expression_if_false;
                      condition：如果条件为 true，则执行 expression_if_true。
                      expression_if_true：当条件为 true 时执行的表达式。
                      expression_if_false：当条件为 false 时执行的表达式。
                */
                ptrs.push_back({ptr, size});
                
                // 随机释放一些内存
                if (rand() % 100 < 75) 
                {  // 75%的概率释放
                    size_t index = rand() % ptrs.size();
                    if (useMemPool) 
                    {
                        MemoryPool::deallocate(ptrs[index].first, ptrs[index].second);
                    } 
                    else 
                    {
                        delete[] static_cast<char*>(ptrs[index].first);
                    }
                    ptrs[index] = ptrs.back();
                    ptrs.pop_back();
                }
            }
            
            // 清理剩余内存
            for (const auto& [ptr, size] : ptrs) 
            {
                if (useMemPool) 
                {
                    MemoryPool::deallocate(ptr, size);
                } 
                else 
                {
                    delete[] static_cast<char*>(ptr);
                }
            }
        };
        
        // 测试内存池
        {
            Timer t;
            std::vector<std::thread> threads;
            
            for (size_t i = 0; i < NUM_THREADS; ++i) 
            {
                threads.emplace_back(threadFunc, true);
            }
            
            for (auto& thread : threads) 
            {
                thread.join();
            }
            
            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
        
        // 测试new/delete
        {
            Timer t;
            std::vector<std::thread> threads;
            
            for (size_t i = 0; i < NUM_THREADS; ++i) 
            {
                threads.emplace_back(threadFunc, false);
            }
            
            for (auto& thread : threads) 
            {
                thread.join();
            }
            
            std::cout << "New/Delete: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
    }
    
    // 4. 混合大小测试
    static void testMixedSizes() 
    {
        constexpr size_t NUM_ALLOCS = 50000;
        const size_t SIZES[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
        
        std::cout << "\nTesting mixed size allocations (" << NUM_ALLOCS 
                  << " allocations):" << std::endl;
        
        // 测试内存池
        {
            Timer t;
            std::vector<std::pair<void*, size_t>> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
                size_t size = SIZES[rand() % 8];
                void* p = MemoryPool::allocate(size);
                ptrs.emplace_back(p, size);
                
                // 批量释放
                if (i % 100 == 0 && !ptrs.empty()) 
                {
                    size_t releaseCount = std::min(ptrs.size(), size_t(20));
                    for (size_t j = 0; j < releaseCount; ++j) 
                    {
                        MemoryPool::deallocate(ptrs.back().first, ptrs.back().second);
                        ptrs.pop_back();
                    }
                }
            }
            
            for (const auto& [ptr, size] : ptrs) 
            {
                MemoryPool::deallocate(ptr, size);
            }
            
            std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
        
        // 测试new/delete
        {
            Timer t;
            std::vector<std::pair<void*, size_t>> ptrs;
            ptrs.reserve(NUM_ALLOCS);
            
            for (size_t i = 0; i < NUM_ALLOCS; ++i) 
            {
                size_t size = SIZES[rand() % 8];
                void* p = new char[size];
                ptrs.emplace_back(p, size);
                
                if (i % 100 == 0 && !ptrs.empty()) 
                {
                    size_t releaseCount = std::min(ptrs.size(), size_t(20));
                    for (size_t j = 0; j < releaseCount; ++j) 
                    {
                        delete[] static_cast<char*>(ptrs.back().first);
                        ptrs.pop_back();
                    }
                }
            }
            
            for (const auto& [ptr, size] : ptrs) 
            {
                delete[] static_cast<char*>(ptr);
            }
            
            std::cout << "New/Delete: " << std::fixed << std::setprecision(3) 
                      << t.elapsed() << " ms" << std::endl;
        }
    }
};

int main() 
{
    std::cout << "Starting performance tests..." << std::endl;
    
    // 预热系统
    PerformanceTest::warmup();
    
    // 运行测试
    PerformanceTest::testSmallAllocation();
    PerformanceTest::testMultiThreaded();
    PerformanceTest::testMixedSizes();
    
    return 0;
}