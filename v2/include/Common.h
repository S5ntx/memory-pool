#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace Kama_memoryPool 
{
// 对齐数和大小定义
constexpr size_t ALIGNMENT = 8;
constexpr size_t MAX_BYTES = 256 * 1024; // 256KB
constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT等于指针void*的大小

// 内存块头部信息
struct BlockHeader
{
    size_t size; // 内存块大小
    bool   inUse; // 使用标志
    BlockHeader* next; // 指向下一个内存块
};

// 大小类管理
class SizeClass 
{
public:
    static size_t roundUp(size_t bytes)
    {
        return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);   // 将 bytes 向上对齐到 ALIGNMENT 的倍数
        // 按位取反（~）  按位与运算（&）
        // ALIGNMENT 的值是 8，所以 ALIGNMENT - 1 就是 7，也就是 00000111（二进制）
        // ~(ALIGNMENT - 1)是对 7 进行按位取反（~），也就是将 00000111 变成 11111000
        // ~(ALIGNMENT - 1) 结果是一个值，所有低位都是 0，高位是 1，它用于去除原始地址中不满足对齐要求的低位部分
        // & ~(ALIGNMENT - 1) 将 (bytes + ALIGNMENT - 1) 中的低位部分清零，以确保返回值是 ALIGNMENT 的倍数
    }    
    static size_t getIndex(size_t bytes)
    {   
        // 确保bytes至少为ALIGNMENT
        bytes = std::max(bytes, ALIGNMENT);
        // 向上取整后-1
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
};

} // namespace memoryPool