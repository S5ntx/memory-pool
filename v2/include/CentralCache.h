#pragma once
#include "Common.h"
#include <mutex>

namespace Kama_memoryPool
{

class CentralCache
{
public:
    static CentralCache& getInstance()
    {
        static CentralCache instance;
        return instance;
    }

    void* fetchRange(size_t index);
    void returnRange(void* start, size_t size, size_t bytes);

private:
    // 相互是还所有原子指针为nullptr
    CentralCache()                                                   // 构造函数
    {
        for (auto& ptr : centralFreeList_)
        {
            ptr.store(nullptr, std::memory_order_relaxed);           // 确保每个指针初始为空
            // store() 是一个专门设计来在多线程环境下进行线程安全赋值的方法
        }
        // 初始化所有锁
        for (auto& lock : locks_)
        {
            lock.clear();
        }
        /*
            // 我们为什么不直接 centralFreeList_.fill(nullptr); lock.fill(0)?
            // 没有使用 std::atomic_flag 提供的原子操作接口，因此可能会导致线程安全问题
            // 特别是在对共享内存进行修改时，原子操作是必要的
        */
    }
    /*
       构造函数初始化了成员变量，但没有定义析构函数 可以吗？
       可以 
       // 1. 没有动态分配内存或其他资源
        CentralCache 类的成员变量 centralFreeList_ 和 locks_ 都是 栈上的数据结构，并且没有显式地动态分配内存
        这些成员变量的生命周期由 CentralCache 对象的生命周期管理，当 CentralCache 对象销毁时，这些栈分配的成员变量也会自动销毁。

       // 2. 没有资源管理的需求
        无堆内存分配：类内部没有使用 new 或 malloc 来分配堆内存，这意味着没有需要在析构时释放的动态内存

       // 3. 标准容器管理资源
        你使用了 std::array 这样的标准容器，它们会在对象销毁时自动清理资源。
        比如，std::atomic 和 std::atomic_flag 等原子类型也会自动清理资源，因此不需要手动管理它们的生命周期。

        // 4. 析构函数的缺省行为
        如果你没有显式定义析构函数，编译器会自动生成一个默认的析构函数。
    */

    // 从页缓存获取内存
    void* fetchFromPageCache(size_t size);

private:
    // 中心缓存的自由链表
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;

    // 用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
    // std::atomic_flag 是一个无符号标志，它仅包含两种状态：设置（1） 和 未设置（0）。它是专门用来实现 轻量级锁 的。
    /*
        // std::atomic_flag 主要有两个函数：
        test_and_set：原子地设置标志，并返回设置前的值。
        test_and_set 是 std::atomic_flag 提供的原子操作，
        首先检查标志位是否已经设置（即是否已经加锁），如果没有设置，就将其设置为 "已设置" 并返回旧值（之前是否已经设置过）
        
        clear：原子地清除标志。
     */
};

} // namespace memoryPool