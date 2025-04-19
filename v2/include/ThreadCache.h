#pragma once
#include "Common.h"

namespace Kama_memoryPool 
{

// 线程本地缓存
class ThreadCache
{
public:
    // 单例模式 每个线程一个实例
    static ThreadCache* getInstance()
    {
        static thread_local ThreadCache instance; // thread_local 是用来声明线程局部变量的关键字
        // thread_local 会确保每个线程有自己的独立副本，而不会在多个线程之间共享。
        return &instance;
    }

    void* allocate(size_t size);
    void deallocate(void* ptr, size_t size);
private:
    ThreadCache() 
    {
        // 初始化自由链表和大小统计
        freeList_.fill(nullptr);
        freeListSize_.fill(0);
    }
    
    // 从中心缓存获取内存
    void* fetchFromCentralCache(size_t index);
    // 归还内存到中心缓存
    void returnToCentralCache(void* start, size_t size);

    bool shouldReturnToCentralCache(size_t index);
private:
    // 每个线程的自由链表数组
    std::array<void*, FREE_LIST_SIZE>  freeList_; 
    // freeList_ 是一个存储各个大小类自由链表头指针的数组
    
    std::array<size_t, FREE_LIST_SIZE> freeListSize_; // 自由链表大小统计 
    // freeListSize_ 是一个大小为 FREE_LIST_SIZE 的数组，元素类型为 size_t  

    /*
        全局数组和静态数组在未明确初始化时，编译器会将它们的所有元素默认初始化为0。
        freeListSize_ 中每个索引对应的值是 0

        因为线程本地的自由链表（freeList_）在没有内存块分配的情况下是空的。也就是说，线程本地缓存在初始化时并不会立刻有内存
     */ 
};

} // namespace memoryPool