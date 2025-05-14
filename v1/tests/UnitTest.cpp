#include <iostream>
#include <thread>
#include <vector>

#include "../include/MemoryPool.h"

using namespace Kama_memoryPool;

// 测试用例
class P1 
{
    int id_;// 静态数组
};

class P2 
{
    int id_[5];
};

class P3
{
    int id_[10];
};

class P4
{
    int id_[20];
};
/*
   我们为什么不在newElement的时候直接传入数组 而先创造一个类出来？
   //  通过创建一个类来包含数组，你能够将数组作为类的成员变量进行封装，这样可以为数组提供更高层次的抽象，
       使得类的用户（包括其他类和函数）能够通过类提供的接口来操作这些数组，而不必直接与数组本身打交道。

   //  提供更多的操作接口，增加代码的封装性；
       增强类型安全，避免数组越界和错误访问；
       增加代码的可扩展性和灵活性，未来修改时可以减少对外部代码的影响；
       遵循面向对象的设计原则，使得代码更加符合 OOP 模型，易于维护和理解。
*/

// 单轮次申请释放次数 线程数 轮次
void BenchmarkMemoryPool(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks); // 线程池
	size_t total_costtime = 0;
	for (size_t k = 0; k < nworks; ++k) // 创建 nworks 个线程
	{
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();// 可以返回程序开始执行到当前位置为止，处理器走过的时钟打点数（即"ticks"）
				for (size_t i = 0; i < ntimes; i++)
				{
                    P1* p1 = newElement<P1>(); // 内存池对外接口
                    deleteElement<P1>(p1);
                    P2* p2 = newElement<P2>();
                    deleteElement<P2>(p2);
                    P3* p3 = newElement<P3>();
                    deleteElement<P3>(p3);
                    P4* p4 = newElement<P4>();
                    deleteElement<P4>(p4);
				}
				size_t end1 = clock();

				total_costtime += end1 - begin1;
			}
		});
		/*
		   [捕获列表](参数列表) -> 返回类型 { 函数体}

		   当捕获列表中只有 & 时，意味着捕获外部作用域中的所有变量 按引用 捕获。
		   换句话说，lambda 内部的所有变量都可以通过引用访问外部变量。你无需列出具体的变量名，只要使用 & 就会将外部所有变量通过引用传递给 lambda

           [&]：捕获外部作用域中的所有变量，按引用捕获。
           [=]：捕获外部作用域中的所有变量，按值捕获。
           [x]：按值捕获变量 x。
           [&x]：按引用捕获变量 x。
           [this]：捕获类的 this 指针（通常用于成员函数中的 lambda）。

		   这段代码的 lambda 并不需要返回值，它的主要目的是执行计算和更新外部状态（比如 total_costtime）

		*/
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%lu个线程并发执行%lu轮次，每轮次newElement&deleteElement %lu次，总计花费：%lu ms\n", nworks, rounds, ntimes, total_costtime);
}

void BenchmarkNew(size_t ntimes, size_t nworks, size_t rounds)
{
	std::vector<std::thread> vthread(nworks);
	size_t total_costtime = 0;
	for (size_t k = 0; k < nworks; ++k)
	{
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; ++j)
			{
				size_t begin1 = clock();
				for (size_t i = 0; i < ntimes; i++)
				{
                    P1* p1 = new P1;
                    delete p1;
                    P2* p2 = new P2;
                    delete p2;
                    P3* p3 = new P3;
                    delete p3;
                    P4* p4 = new P4;
                    delete p4;
				}
				size_t end1 = clock();
				
				total_costtime += end1 - begin1;
			}
		});
	}
	for (auto& t : vthread)
	{
		t.join();
	}
	printf("%lu个线程并发执行%lu轮次，每轮次malloc&free %lu次，总计花费：%lu ms\n", nworks, rounds, ntimes, total_costtime);
}

int main()
{
    HashBucket::initMemoryPool(); // 使用内存池接口前一定要先调用该函数
	BenchmarkMemoryPool(100, 5, 10); // 测试内存池
	std::cout << "===========================================================================" << std::endl;
	std::cout << "===========================================================================" << std::endl;
	BenchmarkNew(100, 5, 10); // 测试 new delete
	
	/*
	HashBucket::initMemoryPool(); // 使用内存池接口前一定要先调用该函数
	BenchmarkMemoryPool(100, 2, 10); // 测试内存池
	std::cout << "===========================================================================" << std::endl;
	std::cout << "===========================================================================" << std::endl;
	BenchmarkNew(100, 2, 10); // 测试 new delete
	*/

	/*
	HashBucket::initMemoryPool(); // 使用内存池接口前一定要先调用该函数
	BenchmarkMemoryPool(100, 1, 10); // 测试内存池
	std::cout << "===========================================================================" << std::endl;
	std::cout << "===========================================================================" << std::endl;
	BenchmarkNew(100, 1, 10); // 测试 new delete
	*/
	return 0;
}