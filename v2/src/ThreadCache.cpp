#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace Kama_memoryPool
{

void* ThreadCache::allocate(size_t size)
{
    // 处理0大小的分配请求
    if (size == 0)
    {
        size = ALIGNMENT; // 至少分配一个对齐大小
    }
    
    if (size > MAX_BYTES)
    {
        // 大对象直接从系统分配
        return malloc(size);
    }

    size_t index = SizeClass::getIndex(size);
     
    // 更新自由链表大小
    freeListSize_[index]--;
    
    // 检查线程本地自由链表
    // 如果 freeList_[index] 不为空，表示该链表中有可用内存块
    if (void* ptr = freeList_[index])
    {
        freeList_[index] = *reinterpret_cast<void**>(ptr); // 将freeList_[index]指向的内存块的下一个内存块地址（取决于内存块的实现）
        /*
            // 这里的 ptr 原本是 void* 类型的指针，意味着它指向一个不确定类型的数据 
            // 编译器无法知道该指针实际上指向什么类型的数据
            // 而 reinterpret_cast<void**>(ptr) 将它转换为 void** 类型，意味着你将 ptr 视为一个指向指针的指针 我们需要访问 ptr 所指向的内存块中的 next 字段。
            // 通过解引用转换后的指针，我们能够得到 ptr 所指向的内存块中的 next 字段，即下一个内存块的地址
        */
       
        /*
            为什么不能直接访问 ptr->next?
            在 ptr 是 void* 类型时， void* 是不具备成员访问操作的。                       freeList_ 中的每个指针并不知道自己指向的是什么类型
            编译器不知道 ptr 实际指向的是一个什么类型的数据结构，所以不能直接访问 next 字段
            必须通过类型转换，告诉编译器 ptr 实际上指向的是一个包含 next 字段的结构体（比如 BlockHeader）

        */
        
        return ptr;
    }
    
    // 如果线程本地自由链表为空，则从中心缓存获取一批内存
    return fetchFromCentralCache(index);
}

void ThreadCache::deallocate(void* ptr, size_t size)
{
    if (size > MAX_BYTES)
    {
        free(ptr);
        return;
    }

    size_t index = SizeClass::getIndex(size);

    // 插入到线程本地自由链表
    *reinterpret_cast<void**>(ptr) = freeList_[index]; // 将 freeList_[index] 指向的内存块地址存储到 ptr 指向的内存块的 next 字段中。
    
    freeList_[index] = ptr;

    // 更新自由链表大小
    freeListSize_[index]++; // 增加对应大小类的自由链表大小

    // 判断是否需要将部分内存回收给中心缓存
    if (shouldReturnToCentralCache(index))
    {
        returnToCentralCache(freeList_[index], size);
    }
}

// 判断是否需要将内存回收给中心缓存
bool ThreadCache::shouldReturnToCentralCache(size_t index)
{
    // 设定阈值，例如：当自由链表的大小超过一定数量时
    size_t threshold = 64; // 例如，64个内存块
    return (freeListSize_[index] > threshold);
}

void* ThreadCache::fetchFromCentralCache(size_t index)
{
    // 从中心缓存批量获取内存
    void* start = CentralCache::getInstance().fetchRange(index);
    if (!start) return nullptr;

    // 取一个返回，其余放入自由链表
    void* result = start;
    freeList_[index] = *reinterpret_cast<void**>(start);
    
    // 更新自由链表大小
    size_t batchNum = 0;
    void* current = start; // 从start开始遍历

    // 计算从中心缓存获取的内存块数量
    while (current != nullptr)
    {
        batchNum++;
        current = *reinterpret_cast<void**>(current); // 遍历下一个内存块
    }

    // 更新freeListSize_，增加获取的内存块数量
    freeListSize_[index] += batchNum;
    
    return result;
}

void ThreadCache::returnToCentralCache(void* start, size_t size)
{
    // 根据大小计算对应的索引
    size_t index = SizeClass::getIndex(size);

    // 获取对齐后的实际块大小
    size_t alignedSize = SizeClass::roundUp(size);

    // 计算要归还内存块数量
    size_t batchNum = freeListSize_[index];
    if (batchNum <= 1) return; // 如果只有一个块，则不归还

    // 保留一部分在ThreadCache中（比如保留1/4）
    size_t keepNum = std::max(batchNum / 4, size_t(1));
    size_t returnNum = batchNum - keepNum;

    // 将内存块串成链表
    char* current = static_cast<char*>(start);
    // 通过将 void* 转换为 char*，你可以按字节访问内存

    // 使用对齐后的大小计算分割点
    char* splitNode = current;                                   //splitNode 初始时是指向一个内存块的指针（char*）
    for (size_t i = 0; i < keepNum - 1; ++i) 
    {
        splitNode = reinterpret_cast<char*>(*reinterpret_cast<void**>(splitNode)); 
        // 首先把 splitNode 强制转换为 void** 类型，也就是将 splitNode 当作一个指向 void* 的指针。
        // 这样，我们可以间接地访问 splitNode 指向位置的内存地址。

        // *reinterpret_cast<void**>(splitNode)：通过解引用后，取得 splitNode 指向内存位置的内容，也就是它所指向的下一个内存块的地址。

        // reinterpret_cast<char*>(...)：然后将这个地址再转换回 char* 类型，表示下一个内存块的位置。

        if (splitNode == nullptr) 
        {
            // 如果链表提前结束，更新实际的返回数量
            returnNum = batchNum - (i + 1);
            break;
        }
    }

    if (splitNode != nullptr) 
    {
        // 将要返回的部分和要保留的部分断开
        void* nextNode = *reinterpret_cast<void**>(splitNode);
        *reinterpret_cast<void**>(splitNode) = nullptr; // 断开连接

        // 更新ThreadCache的空闲链表
        freeList_[index] = start;

        // 更新自由链表大小
        freeListSize_[index] = keepNum;

        // 将剩余部分返回给CentralCache
        if (returnNum > 0 && nextNode != nullptr)
        {
            CentralCache::getInstance().returnRange(nextNode, returnNum * alignedSize, index);
        }
    }
}


} // namespace memoryPool