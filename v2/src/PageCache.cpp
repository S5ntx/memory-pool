#include "PageCache.h"
#include <sys/mman.h>
#include <cstring>

namespace Kama_memoryPool
{

void* PageCache::allocateSpan(size_t numPages)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找合适的空闲span
    // lower_bound函数返回第一个大于等于numPages的元素的迭代器
    auto it = freeSpans_.lower_bound(numPages);
    /*
        // lower_bound 是 C++ 标准库 std::map 或 std::multimap 中的一个成员函数
           它的作用是查找一个容器中第一个大于或等于给定值的元素，并返回该元素的迭代器

        // lower_bound(numPages) 返回一个指向 freeSpans_ 中第一个键值大于或等于 numPages 的元素的迭代器。
           如果没有找到这样的元素（即所有元素的键值都大于 numPages），则返回 freeSpans_.end()
    
    */
    if (it != freeSpans_.end())
    {
        Span* span = it->second;

        // 将取出的span从原有的空闲链表freeSpans_[it->first]中移除
        if (span->next)
        {
            freeSpans_[it->first] = span->next;  
            //如果 span 后面还有空闲的 Span（即 span->next 不为 nullptr），则将 freeSpans_ 中当前页数对应的链表头指向下一个 Span
        }
        else
        {
            freeSpans_.erase(it);
            //如果 span 后面没有其他空闲内存（即 span->next 为 nullptr），则直接从 freeSpans_ 中删除当前元素
        }

        // 如果span大于需要的numPages则进行分割
        if (span->numPages > numPages) 
        {
            Span* newSpan = new Span;                      // 创建一个新的 Span，表示超出 numPages 的部分。
            newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
            // ：numPages 表示原始 Span 所包含的页数，而 PAGE_SIZE 是每一页的大小。numPages * PAGE_SIZE 计算出 numPages 页所占的字节数
            newSpan->numPages = span->numPages - numPages;
            //  span->numPages：原始 Span 所有的页数。       // numPages：我们需要的页数。
            newSpan->next = nullptr;
            /* 
                //  将新 Span 的 next 指针设置为 nullptr，表示当前的新 Span 后面没有更多的空闲内存块。
                    在 freeSpans_ 中，空闲的 Span 是通过链表形式管理的，next 指针指向下一个空闲的 Span。
                    当我们创建一个新的 Span 来表示剩余的内存块时，因为它是一个新的空闲内存块且目前没有其他空闲块与它相连，所以将 next 设置为 nullptr

                //  为了确保代码的安全性和明确性，最好写上 newSpan->next = nullptr;，这是一种良好的初始化做法，能够防止潜在的内存错误和未定义行为
            */
            
            // 将超出部分放回空闲Span*列表头部
            auto& list = freeSpans_[newSpan->numPages];
            newSpan->next = list;                         // 将新 Span 的 next 指针指向当前链表的头部
            list = newSpan;                               // 更新了链表头指针 list，使其指向 newSpan，这时 newSpan 成为新的链表头

            span->numPages = numPages;
        }

        // 记录span信息用于回收
        spanMap_[span->pageAddr] = span;
        return span->pageAddr;
    }

    // 没有合适的span，向系统申请
    void* memory = systemAlloc(numPages);
    if (!memory) return nullptr;

    // 创建新的span
    Span* span = new Span;
    span->pageAddr = memory;
    span->numPages = numPages;
    span->next = nullptr;

    // 记录span信息用于回收
    spanMap_[memory] = span;
    return memory;
}

void PageCache::deallocateSpan(void* ptr, size_t numPages) // void* ptr：这是指向要释放的内存区域的指针  size_t numPages：表示释放的内存的页数
{
    std::lock_guard<std::mutex> lock(mutex_);

    // 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
    auto it = spanMap_.find(ptr);
    if (it == spanMap_.end()) return;

    Span* span = it->second;

    // 尝试合并相邻的span
    void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;  // nextAddr 计算出下一个相邻内存块的起始地址
    auto nextIt = spanMap_.find(nextAddr);
    
    if (nextIt != spanMap_.end())
    {
        Span* nextSpan = nextIt->second;
        
        // 1. 首先检查nextSpan是否在空闲链表中
        bool found = false;                                             // found 标志表示是否找到了 nextSpan 在空闲链表中的位置。
        auto& nextList = freeSpans_[nextSpan->numPages];
        
        // 检查是否是头节点
        if (nextList == nextSpan)
        {
            nextList = nextSpan->next;
            found = true;
        }
        else if (nextList) // 只有在链表非空时才遍历
        {
            Span* prev = nextList;
            while (prev->next)                                        // 如果 nextList 非空，遍历空闲链表，查找 nextSpan
            {
                if (prev->next == nextSpan)
                {   
                    // 将nextSpan从空闲链表中移除
                    prev->next = nextSpan->next;
                    found = true;
                    break;
                }
                prev = prev->next;
            }
        }

        // 2. 只有在找到nextSpan的情况下才进行合并
        if (found)
        {
            // 合并span
            span->numPages += nextSpan->numPages;
            spanMap_.erase(nextAddr);
            delete nextSpan;
        }
    }

    // 将合并后的span通过头插法插入空闲列表
    auto& list = freeSpans_[span->numPages];
    span->next = list;
    list = span;
}

void* PageCache::systemAlloc(size_t numPages)
{
    size_t size = numPages * PAGE_SIZE;

    // 使用mmap分配内存
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;
    /*
      // mmap 是一个系统调用，用于将文件或设备  映射到进程的虚拟内存地址空间，或者用于分配匿名的内存
         通常用来进行大块内存的分配或内存映射，能够比常规的 malloc 或 new 更灵活地控制内存的分配
         void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset);
         // addr：建议的内存地址，通常传入 nullptr，表示由系统决定分配的地址。
         // length：需要映射的内存区域大小（字节数）。
         // prot：指定映射区域的访问权限（读、写等）。                                 PROT_READ | PROT_WRITE 表示映射区域既可读又可写
         // flags：指定映射的类型，例如是否匿名映射、是否共享映射等。                   MAP_PRIVATE | MAP_ANONYMOUS：表示内存是私有的，并且与文件无关（匿名映射）
         // fd：映射的文件描述符。如果是匿名映射，这个参数通常设为 -1。
         // offset：文件的偏移量（对于文件映射），如果是匿名映射，通常设为 0。
      // 如果 mmap 成功，它会返回一个指向分配内存的指针。如果失败，返回 MAP_FAILED，表示映射失败
    */

    // 清零内存
    memset(ptr, 0, size);
    return ptr;
    /*
       // memset 是一个 C 标准库函数，用于将一块内存区域的内容设置为指定的值。其函数签名如下： 
           void* memset(void* ptr, int value, size_t num);
           ptr：要填充的内存块的起始地址。                                       // 填充的值是 0，这表示将内存区域中的所有字节都设置为 0。
           value：要填充到内存块的值，通常是一个字节值（int 类型，但只使用低字节）。
           num：要填充的字节数。                                                // size：要设置的字节数，即分配的内存大小
       // 这行代码的目的是将分配的内存初始化为 0，这样分配到的内存不会包含任何未初始化的数据
    */
}

} // namespace memoryPool