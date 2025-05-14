#pragma once                //GCC、Clang、MSVC 
/*
    // #pragma once 等同于

    // ifndef MEMORY_POOL_H      如果没有定义 MEMORY_POOL_H 宏，就执行下面的代码
    // #define MEMORY_POOL_H     表示定义了这个宏 MEMORY_POOL_H，告诉编译器“我已经包含过这个头文件了”

    // ...

    // endif                     和 #ifndef 配对，表示条件编译的结束 
    // 防止头文件被重复包含


    // 当你#include <>的时候
    // 编译器做的不是“记录你引用了哪个文件”，而是在预处理阶段直接把这个文件的内容完整复制过来

    // 多次声明（如 void f();）不会报错
    // 多次定义类/函数体  和 多次定义全局变量 会报错


*/

#include <atomic>           // 提供原子操作的支持。
#include <cassert>          // 提供断言宏，用于调试时检查假设。
#include <cstdint>          // 提供标准的整数类型，如int32_t、uint64_t等。
#include <iostream>
#include <memory>           // 用于智能指针等内存管理功能
#include <mutex>            // 用于线程同步

namespace Kama_memoryPool    // 定义一个名为 Kama_memoryPool 的命名空间 在这个命名空间下的类和函数将属于这个内存池项目，避免与其他项目的命名冲突
{
#define MEMORY_POOL_NUM 64   // 表示内存池中槽的数量
#define SLOT_BASE_SIZE 8     // 内存池槽的基本大小（8字节）
#define MAX_SLOT_SIZE 512    // 内存池槽的最大大小（512字节）


/* 
   具体内存池的槽大小没法确定，因为每个内存池的槽大小不同(8的倍数)
   所以这个槽结构体的sizeof 不是实际的槽大小 
*/

// Slot 结构体代表一个内存池的槽
struct Slot                  //
{
    std::atomic<Slot*> next; // 原子指针   
    /*
        // 每个 Slot 对象都有一个 next 指针， 指向链表中的下一个空闲槽

        // std::atomic 保证这个指针在多线程环境下的安全性。
        // 在多线程编程中，原子指针保证了不会出现并发访问时的竞态条件
    */

};
/* 
   我们为什么要这么做？

   如果你要在多线程下操作 freeList_（头指针）链表，比如入队/出队，就需要修改：当前节点的 next 、全局 freeList_ 头指针
   这些都是非原子的操作！ 在多个线程同时访问时，你根本没法保证 next 设置的是哪个版本，数据会错乱或崩溃。

   // ***原子性意味着操作不能被中断，保证在多线程环境下的正确性。****
*/

class MemoryPool
{
public:
    MemoryPool(size_t BlockSize = 4096);
    ~MemoryPool();
    
    void init(size_t);

    void* allocate();
    void deallocate(void*);  
    /*
        // void* 是 C++ 中的 "指向未知类型的指针"，它被称为**“空指针类型”**（void pointer）
        // 可以将任何类型的指针（如 int*、float*、char* 等）转换为 void* 类型 反之亦然
        // 但是，你不能直接对 void* 进行解引用，必须将其转换为一个已知类型的指针后才能访问其指向的数据。
    */

private:
    void allocateNewBlock();                  // 分配一个新的内存块     
    size_t padPointer(char* p, size_t align); // 确保指针对齐 

    // 使用CAS操作进行无锁入队和出队
    bool pushFreeList(Slot* slot);            // 向空闲槽列表中添加槽
    Slot* popFreeList();                      // 向空闲槽列表中取出槽
private:
    int                 BlockSize_;         // 内存块大小
    int                 SlotSize_;          // 槽大小
    Slot*               firstBlock_;        // 指向内存池管理的首个实际内存块
    Slot*               curSlot_;           // 指向当前未被使用过的槽
    std::atomic<Slot*>  freeList_;          // 指向空闲的槽(被使用过后又被释放的槽)
    Slot*               lastSlot_;          // 作为当前内存块中最后能够存放元素的位置标识(超过该位置需申请新的内存块)
    //std::mutex          mutexForFreeList_; // 保证freeList_在多线程中操作的原子性
    std::mutex          mutexForBlock_;      // 保证多线程情况下避免不必要的重复开辟内存导致的浪费行为
};

class HashBucket
{
public:
    static void initMemoryPool();
    static MemoryPool& getMemoryPool(int index);          // 获取指定索引的内存池

    static void* useMemory(size_t size)                   // 根据内存大小选择合适的内存池，分配内存
    {
        if (size <= 0)
            return nullptr;
        if (size > MAX_SLOT_SIZE) // 大于512字节的内存，则使用new
            return operator new(size);

        // 相当于size / 8 向上取整（因为分配内存只能大不能小
        return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
        // 如果直接size/SLOT_BASE_SIZE 当size是SLOT_BASE_SIZE整数倍的时候  会出现错误  破坏这个区间映射
    }

    static void freeMemory(void* ptr, size_t size)
    {
        if (!ptr)
            return;
        if (size > MAX_SLOT_SIZE)
        {
            operator delete(ptr);
            return;
        }

        getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
    }

    template<typename T, typename... Args> 
    friend T* newElement(Args&&... args);
    
    template<typename T>
    friend void deleteElement(T* p);
};

template<typename T, typename... Args>
T* newElement(Args&&... args)
{
    T* p = nullptr;
    // 根据元素大小选取合适的内存池分配内存
    if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        // 在分配的内存上构造对象
        new(p) T(std::forward<Args>(args)...);
        /*
        // std::forward<Args>(args)... 通过完美转发将原始参数传递给构造函数。std::forward 保证参数的左值/右值特性被正确保留
        完美转发的作用是：
        如果你传递的是一个 左值，那么 std::forward 会将其转发为左值引用。
        如果你传递的是一个 右值，那么 std::forward 会将其转发为右值引用。

        Args 是一个 包裹类型，它包含了所有传递给 newElement 函数的参数类型。
        args... 是 函数模板参数包，即原始传递给 newElement 函数的参数。
        std::forward<Args>(args)... 会以正确的方式（保留参数的左值或右值特性）将参数传递给构造函数。

3. 为什么
        */
        
    return p;
}
/*
    //  template<typename T, typename... Args> 这是 模板声明，表示 newElement 是一个模板函数。
    //  Args... 是一个可变模板参数包 表示这个函数可以接受多个类型的参数，并将它们传递给构造函数
    //  Args&&... args 是 完美转发（Perfect Forwarding）的技巧，使得你传递给 newElement 的参数能够保持其原有类型（包括左值和右值的区分）

    //  T 是一个模板类型参数，表示你将创建的对象类型 
    //  表示该函数返回一个指向 T 类型的指针
*/

template<typename T>
void deleteElement(T* p)
{
    // 对象析构
    if (p)
    {
        p->~T();
         // 内存回收
        HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
    }
}

} // namespace memoryPool
