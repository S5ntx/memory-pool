#include "../include/MemoryPool.h"
/*
   // #include "MemoryPool.h"：默认查找当前目录下的文件。
   // #include "../include/MemoryPool.h"：查找当前目录的父目录中的 include 文件夹下的 MemoryPool.h 文件。
   // ..：表示“当前目录的父目录”
*/

namespace Kama_memoryPool 
{
MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_ (BlockSize)      // 每个内存块的大小
    , SlotSize_ (0)               // 槽大小
    , firstBlock_ (nullptr)       // 指向内存池管理的首个实际内存块
    , curSlot_ (nullptr)          // 指向当前未被使用过的槽
    , freeList_ (nullptr)         // 指向空闲的槽(被使用过后又被释放的槽)
    , lastSlot_ (nullptr)         // 作为当前内存块中最后能够存放元素的位置标识(超过该位置需申请新的内存块)
{}
/* 
   如果成员变量是在构造时立即需要初始化的，通常会在构造函数的初始化列表中进行初始化。
   如果成员变量在后续的成员函数中会根据某些条件进行初始化，构造函数就不需要显式初始化它们
*/
MemoryPool::~MemoryPool()
{
    // 把连续的block删除
    Slot* cur = firstBlock_;  // firstBlock_ 指向你通过 operator new 分配的内存块头部
    while (cur)
    {
        Slot* next = cur->next;
        operator delete(reinterpret_cast<void*>(cur));
        /*
        // 等同于 free(reinterpret_cast<void*>(firstBlock_));
        // 转化为 void 指针，因为 void 类型不需要调用析构函数，只释放空间
        // 顺着 next 遍历所有 block 把每一个 cur 转成 void*，然后 operator delete()；
        // 这些块里不需要调用析构函数，因为你用的是原始内存块（不是 new 出来的对象数组）
        */
        cur = next;
    }
}

void MemoryPool::init(size_t size)
{
    assert(size > 0);
    // assert 是一个“调试断言”，意思是“我假设这里必须成立，如果不成立，我宁愿程序立刻崩溃以便发现 bug”
    SlotSize_ = size;
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate()
{
    // 优先使用空闲链表中的内存槽
    Slot* slot = popFreeList();
    if (slot != nullptr)
        return slot;

    Slot* temp;
    {   
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if (curSlot_ >= lastSlot_)
        {
            // 当前内存块已无内存槽可用，开辟一块新的内存
            allocateNewBlock();
        }
    
        temp = curSlot_;
        // 这里不能直接 curSlot_ += SlotSize_ 因为curSlot_是Slot*类型，所以需要除以SlotSize_再加1
        curSlot_ += SlotSize_ / sizeof(Slot);
    }
    
    return temp; 
}

void MemoryPool::deallocate(void* ptr)
{
    if (!ptr) return;

    Slot* slot = reinterpret_cast<Slot*>(ptr);
    pushFreeList(slot);
    // 当一个槽被释放时，它的 next 指针会被设置为之前的 freeList 头节点，然后更新 freeList。
}

void MemoryPool::allocateNewBlock()
{   
    //std::cout << "申请一块内存块，SlotSize: " << SlotSize_ << std::endl;
    // 头插法插入新的内存块
    void* newBlock = operator new(BlockSize_);           // void* 表示这块内存是通用的，不指定类型
    reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    size_t paddingSize = padPointer(body, SlotSize_); // 计算对齐需要填充内存的大小
    curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);

    // 超过该标记位置，则说明该内存块已无内存槽可用，需向系统申请新的内存块
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);
    /*
    // BlockSize_ - SlotSize_ 是倒数第二个槽的起始位置。

    // 如果你想让 lastSlot_ 指向 最后一个槽的结束位置，就需要再加上 +1。
    // 实际上，+1 是一个精确的偏移量，指向槽结束后的那个位置（即内存块的边界）。
    */
 
    freeList_ = nullptr;

    // reinterpret_cast 用于操作裸内存
}

// 让指针对齐到槽大小的倍数位置
size_t MemoryPool::padPointer(char* p, size_t align)
{
    // align 是槽大小
    return (align - reinterpret_cast<size_t>(p)) % align;
}

// 实现无锁入队操作
bool MemoryPool::pushFreeList(Slot* slot)
{
    while (true)
    {
        // 获取当前头节点
        Slot* oldHead = freeList_.load(std::memory_order_relaxed); // .load()：读取原子变量的当前值
        // 将新节点的 next 指向当前头节点
        slot->next.store(oldHead, std::memory_order_relaxed);      // .store(val)：写入值

        // 尝试将新节点设置为头节点
        if (freeList_.compare_exchange_weak(oldHead, slot,
         std::memory_order_release, std::memory_order_relaxed))
        {
            return true;
        }
        /*
        
        //.compare_exchange_weak(old, new)： 如果当前值 == old → 改成 new → 返回 true； 否则 → 返回 false；
        // 经典的 CAS（Compare-And-Swap）操作！

        // memory_order 控制并发下读写的执行顺序与安全性       一种编译器/CPU 指令的“提示”或“约束”
        // memory_order_relaxed 最宽松，无序
        // memory_order_release 写屏障，之前的写不能被延后

        // release + acquire 配对就像设置了“读写屏障”，强制顺序不会错！

        // 失败：说明另一个线程可能已经修改了 freeList_
        // CAS 失败则重试
        */
       
    }
}

// 实现无锁出队操作
Slot* MemoryPool::popFreeList()
{
    while (true)
    {
        Slot* oldHead = freeList_.load(std::memory_order_acquire); 
        /*
           // acquire 确保“后面所有读操作”必须在它之后执行；
           std::atomic::load 是 C++ 中的一个函数，用于读取 std::atomic 对象的值。
           这个函数会以原子方式读取存储的值，这意味着在多线程环境中，这个操作是线程安全的。
        */
        
        if (oldHead == nullptr)
            return nullptr; // 队列为空

        // 在访问 newHead 之前再次验证 oldHead 的有效性
        Slot* newHead = nullptr;
        try                              // try 块用来捕获可能会抛出的异常
        {
            newHead = oldHead->next.load(std::memory_order_relaxed);
            /*
                oldHead->next.load(std::memory_order_relaxed) 这行代码本身是不会抛出异常的。
                但是，如果 oldHead 为空或指向无效内存，这可能会导致其他异常或错误（如内存访问错误）
            */
        }
        catch(...)                       // catch 块用来处理这些异常 ... 捕获所有类型的异常
        {
            // 如果返回失败，则continue重新尝试申请内存
            continue;
        }
        
        // 尝试更新头结点
        // 原子性地尝试将 freeList_ 从 oldHead 更新为 newHead
        if (freeList_.compare_exchange_weak(oldHead, newHead,
         std::memory_order_acquire, std::memory_order_relaxed))
        {
            return oldHead;
            /*
                compare_exchange_weak 是一种 原子比较并交换（CAS，Compare-and-Swap）操作。它尝试将 freeList_ 的当前值（oldHead）替换为 newHead
                只有在 freeList_ 的当前值与 oldHead 相等时，操作才会成功。也就是说，如果 freeList_ 的当前值等于 oldHead，那么它会被替换成 newHead
                
            */
        }
        /*
           // 失败：说明另一个线程可能已经修改了 freeList_
           // CAS 失败则重试
           如果 compare_exchange_weak 失败了，说明其他线程已经修改了 freeList_ 的值，这可能意味着 oldHead 变得过时或者被其他线程修改了。
           在这种情况下，程序会重新开始整个过程，重新读取 freeList_，获取新的 oldHead，然后再尝试一次。换句话说，如果 CAS 操作失败，你会重新执行 while (true) 循环中的操作，重新尝试直到成功为止。
           这是一个典型的“自旋锁”机制，持续不断地进行尝试，直到更新成功。
        */
        
    }
}


void HashBucket::initMemoryPool()
{
    for (int i = 0; i < MEMORY_POOL_NUM; i++)
    {
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE); // “引用+函数链式调用”
    }
}   

// 单例模式
MemoryPool& HashBucket::getMemoryPool(int index)
{
    static MemoryPool memoryPool[MEMORY_POOL_NUM];
    /*
       // static 保证内存池的唯一性与持续性  创建并初始化 memoryPool[] 后续所有调用：都使用这个已经初始化好的那一份
       // 静态局部变量，只初始化一次，在整个程序运行期间全局保留 
       // 声明了一个MemoryPool 对象的数组，数组长度是常量 MEMORY_POOL_NUM

       // 整个 memoryPool[] 这块静态数组，只有第一次调用 getMemoryPool() 函数时才创建，整个程序运行期间只构造一次
       // 不是 一上来程序启动就分配
    */
   
    return memoryPool[index];

    /*                   
     // 按需构造第 N 个池子
    static std::unique_ptr<MemoryPool> memoryPools[64];

    if (!memoryPools[index]) {
        memoryPools[index] = std::make_unique<MemoryPool>();
    }
    return *memoryPools[index];
    */ 
}

} // namespace memoryPool

