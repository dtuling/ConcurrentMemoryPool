#pragma once
#include "Common.h"

class ThreadCache
{
public:
	//申请内存对象
	void* Allocate(size_t size);
	//释放内存对象
	void Deallocate(void* ptr,size_t size);
	// 从中心缓存获取对象
	void* FetchFromCentralCache(size_t index, size_t size);
	// 释放对象时，链表过长时，回收内存回到中心缓存
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _free_list[NFREELISTS];//管理切分好的自由链表(哈希桶)
};

//使用 __declspec(thread) 声明的变量将为每个线程创建一个独立的副本
//每个线程都可以在其独立的存储空间中访问该变量，而不会影响其他线程的副本。
//让这个指针维护thread cache进行内存时申请和释放
static __declspec(thread) ThreadCache* tls_threadcache = nullptr;