#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>

namespace Kama_memoryPool
{

// 每次从PageCache获取span大小（以页为单位）
static const size_t SPAN_PAGES = 8;

void* CentralCache::fetchRange(size_t index)
{
    // 索引检查，当索引大于等于FREE_LIST_SIZE时，说明申请内存过大应直接向系统申请
    if (index >= FREE_LIST_SIZE) 
        return nullptr;

    // 自旋锁保护
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield(); // 添加线程让步，避免忙等待，避免过度消耗CPU
        /*
            //如果 test_and_set 返回 true，即表示锁已被其他线程占用，当前线程就会调用 std::this_thread::yield() 让出 CPU 控制权。
            std::this_thread::yield() 会使当前线程暂停执行，允许其他线程有机会运行，
            这样可以避免一个线程长时间占用 CPU 导致其他线程无法运行，从而造成忙等待（busy-waiting）的问题
            // 这一部分代码的目的是通过自旋锁来确保只有一个线程可以进入临界区，并在锁被占用时通过线程让步来避免 CPU 资源的浪费
        */
    }

    void* result = nullptr;
    try 
    {
        // 尝试从中心缓存获取内存块
        result = centralFreeList_[index].load(std::memory_order_relaxed);
        /*
           // 如果从 centralFreeList_ 获取到的 result 为空，说明缓存中没有足够的内存块，接着会尝试通过 fetchFromPageCache 来从页缓存中获取内存块
           // result 是从页缓存中分配的内存的起始地址
           // 每次对 char* 指针加一个整数，意味着偏移了一个字节
        */
        
        if (!result)
        {
            // 如果中心缓存为空，从页缓存获取新的内存块
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);

            if (!result)
            {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
                // 如果 fetchFromPageCache 也返回空（即页缓存中没有足够的内存），则释放锁并返回 nullptr，表示无法提供内存块
            }

            // 将获取的内存块切分成小块
            char* start = static_cast<char*>(result);
            size_t blockNum = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;
            /*
                //  SPAN_PAGES * PageCache::PAGE_SIZE / size 计算的是 从页缓存中获取到的内存块能够切分成多少个小块
                    SPAN_PAGES 是一个常量，表示内存块在页缓存中的大小
                    PageCache::PAGE_SIZE 是每一页的大小            它是一个固定的常量，表示内存中每个内存页的大小。

                //  SPAN_PAGES * PageCache::PAGE_SIZE：计算从页缓存中分配的总内存量        
            */
            
            if (blockNum > 1) 
            {  // 确保至少有两个块才构建链表
                for (size_t i = 1; i < blockNum; ++i) 
                {
                    void* current = start + (i - 1) * size; // start + (i - 1) * size 通过加上 (i - 1) * size 来获取当前内存块的起始地址
                    void* next = start + i * size;          // start + i * size 会计算出下一个内存块的起始地址，即第 i 个内存块的起始地址
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr; 
                //为最后一个内存块设置“结束标志”，即将最后一个内存块的 next 指针设置为 nullptr，表示链表的结束
                
                // 保存result的下一个节点
                void* next = *reinterpret_cast<void**>(result);
                // 将result与链表断开
                *reinterpret_cast<void**>(result) = nullptr;
                // 更新中心缓存
                centralFreeList_[index].store(next, std::memory_order_release);
            }
        } 
        else// 从缓存中直接获取到内存块  将这个内存块的 next 指针提取出来，并将 result 与链表断开
        {
            // 保存result的下一个节点
            void* next = *reinterpret_cast<void**>(result);
            // 将result与链表断开
            *reinterpret_cast<void**>(result) = nullptr;
            
            // 更新中心缓存
            centralFreeList_[index].store(next, std::memory_order_release);
        }
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
        /*
            // throw 是 C++ 中的关键字
            我们用它来标识程序中出现了错误或者异常情况，并将异常信息传递给程序的其他部分进行处理
            // throw; 会重新抛出当前捕获的异常
            这里 throw 语句会将当前捕获的异常再向上传递 给调用栈中的上一级异常处理程序
        */
    }

    // 释放锁
    locks_[index].clear(std::memory_order_release);
    return result;
}

void CentralCache::returnRange(void* start, size_t size, size_t index)
{
    // 当索引大于等于FREE_LIST_SIZE时，说明内存过大应直接向系统归还
    if (!start || index >= FREE_LIST_SIZE) 
        return;
    /*
        // start 是一个 void* 类型的指针，它表示待归还内存块的起始地址。如果 start 为 nullptr，说明没有有效的内存块需要归还

    */

    while (locks_[index].test_and_set(std::memory_order_acquire)) 
    {
        std::this_thread::yield();
    }

    try 
    {
        // 找到要归还的链表的最后一个节点
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) != nullptr && count < size) {
            end = *reinterpret_cast<void**>(end);
            count++;
        }

        // 将归还的链表连接到中心缓存的链表头部
        void* current = centralFreeList_[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void**>(end) = current;  // 将原链表头接到归还链表的尾部
        centralFreeList_[index].store(start, std::memory_order_release);  // 将归还的链表头设为新的链表头
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
}

void* CentralCache::fetchFromPageCache(size_t size)
{   
    // 1. 计算实际需要的页数
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    // 2. 根据大小决定分配策略
    if (size <= SPAN_PAGES * PageCache::PAGE_SIZE) 
    {
        // 小于等于32KB的请求，使用固定8页
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    } 
    else 
    {
        // 大于32KB的请求，按实际需求分配
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

} // namespace memoryPool