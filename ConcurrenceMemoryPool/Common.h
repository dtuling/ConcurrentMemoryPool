#pragma once
#include <iostream>
#include <vector>
#include <cassert>
#include <thread>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <atomic>
using std::cout;
using std::endl;

static const size_t MAX_BYTES = 256 * 1024;
static const size_t NFREELISTS = 208;
static const size_t PAGE_NUMS = 129;//页数
static const size_t PAGE_SHIFT = 13;//一页8kb  2^13


#ifdef _WIN32
#include <Windows.h>
#else
//...
#endif

//直接去堆上申请按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
	void* ptr = mmap(0, kpage << 13, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
	if (ptr == nullptr)
	{
		printf("malloc fail");
		exit(-1);
	}
	return ptr;
}
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}


static void* &NextObj(void* obj)
{
	return *(void**)obj;
}

//自由链表
class FreeList
{
public:
	//头插
	void Push(void* obj)
	{
		*(void**)obj = _free_list;
		_free_list = obj;
		++_size;
	}
	//头插一段链表
	void PushRound(void * start,void* end,size_t size)
	{
		*(void**)end = _free_list;
		_free_list = start;

	/*	int i = 0;
		void* cur = start;
		while (cur != nullptr)
		{
			cur = NextObj(cur);
			++i;
		}
		if (i == size)
		{
			int x = 0;
		}*/

		_size += size;
	}
	//头删
	void*Pop()
	{
		void* next = *((void**)_free_list);
		void* obj = _free_list;
		_free_list = next;
		--_size;
		return obj;
	}
	//头删一段链表
	void PopRound(void*& start,void*& end,size_t size)
	{
		start = _free_list;
		end = start;
		for (size_t i = 0; i < size - 1; ++i)
		{
			end = NextObj(end);
		}
		_free_list = NextObj(end);
		_size -= size;
		NextObj(end) = nullptr;
	}
	bool IsEmpty()
	{
		return _free_list == nullptr;
	}
	//获取当前链表最大个数
	size_t& MaxSize()
	{
		return _max_size;
	}
	//获取当前链表元素个数
	size_t Size()
	{
		return _size;
	}
private:
	void* _free_list = nullptr;
	size_t _max_size = 1;
	size_t _size = 0;
};


class SizeClass
{
public:
	// 控制在1%-12%左右的内碎片浪费
	// [1,128]					8byte对齐	     freelist[0,16)
	// [129,1024]				16byte对齐		 freelist[16,72)
	// [1025,8*1024]			128byte对齐	     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024]     8*1024byte对齐   freeist[184,208)
	// 
	//static inline size_t _RoundUp(size_t bytes, size_t align)
	//{
	//	size_t align_num = 0;
	//	if (bytes % align == 0)//正好整除
	//	{
	//		align_num = bytes;
	//	}
	//	else //比如7字节，开8字节
	//	{
	//		align_num = ((bytes / align) + 1) * align;
	//	}
	//	return align_num;
	//}
	// 
	// tcmalloc库的写法 更优秀
	static inline size_t _RoundUp(size_t bytes, size_t align)
	{
		return ((bytes + align - 1) & ~(align - 1));
	}
	//根据对象大小计算对齐规则   
	static inline size_t RoundUp(size_t bytes)
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);//按照8字节对齐
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);//按照16字节对齐
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);//按照128字节对齐
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);//按照1024字节对齐
		}
		else if (bytes <= 256 * 1024)
		{
			return _RoundUp(bytes, 8*1024);//按照8*1024字节对齐
		}
		else if(bytes > 256 * 1024)
		{
			return _RoundUp(bytes, 1 << PAGE_SHIFT);
		}
		else
		{
			assert(false);
			return -1;
		}
	}
	
	// [1,128]					8byte对齐	     freelist[0,16)
	// [129,1024]				16byte对齐		 freelist[16,72)
	// [1025,8*1024]			128byte对齐	     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024]     8*1024byte对齐   freeist[184,208)
	//确定桶的位置 
	//大佬库的写法
	static inline size_t _Index(size_t bytes, size_t alignShift)
	{
		return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
	}
	//映射到自由链表的位置
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		//每个链有多少个桶
		static size_t groupArray[5] = { 16, 56, 56, 56 ,24};
		if (bytes <= 128)
		{
			return _Index(bytes, 3);//2^3=8
		}
		else if (bytes <= 1024)
		{
			return _Index(bytes -128, 4) + groupArray[0];//2^4=16
		}
		else if (bytes <= 8 * 1024)
		{
			return _Index(bytes - 1024, 7) + groupArray[0] + groupArray[1];
		}
		else if (bytes <= 64 * 1024)
		{
			return _Index(bytes - 8 * 1024, 10) + groupArray[0] + groupArray[1] + groupArray[2];
		}
		else if (bytes <= 256 * 1024)
		{
			return _Index(bytes - 64 * 1024, 13) + groupArray[0] + groupArray[1] + groupArray[2] + groupArray[3];
		}
		else
		{
			assert(false);
			return -1;
		}
	}


	// 一次thread cache从中心缓存获取多少个
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		// [2, 512]，一次批量移动多少个对象的(慢启动)上限值
		// 小对象一次批量上限高
		// 小对象一次批量上限低
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	// 计算一次向系统获取几个页
	// 单个对象 8byte
	// ...
	// 单个对象 256KB
	static size_t NumMovePage(size_t size)
	{
		size_t num = NumMoveSize(size);
		size_t npage = num * size;

		npage >>= PAGE_SHIFT;
		if (npage == 0)
			npage = 1;

		return npage;
	}
};
	#ifndef _WIN64
		typedef unsigned long long PAGE_ID;
	#elif _WIN32
		typedef size_t PAGE_ID;
	#endif // !_WIN64

struct Span
{
	PAGE_ID _page_id = 0; //页号
	size_t _page_num = 0;//页的数量

	Span* _prev = nullptr;
	Span* _next = nullptr;

	size_t _obj_size = 0;//对象大小 
	size_t _use_count = 0;
	void* _free_list = nullptr;
	bool _is_use = false;//span对象是否被使用
};
//SpanList是一个带头双向循环链表 以sapn为节点 做为Central的成员变量
class SpanList
{
public:
	SpanList()
	{
		_head = new Span;
		_head->_next = _head;
		_head->_prev = _head;
	}
	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}
	
	void Insert(Span* cur, Span* newspan)
	{
		//assert(cur != _head);
		//assert(newspan != nullptr);

		Span* prev = cur->_prev;
		prev->_next = newspan;
		newspan->_prev = prev;
		newspan->_next = cur;
		cur->_prev = newspan;
	}
	void Erase(Span* cur)
	{
		assert(cur != nullptr);
		assert(cur != _head);

		Span* prev = cur->_prev;
		Span* next = cur->_next;
		prev->_next = next;
		next->_prev = prev;
	}
	bool Empty()
	{
		return _head->_next == _head;
	}
	void PushFront(Span* newspan)
	{
		assert(newspan != nullptr);
		Insert(Begin(), newspan);
	}
	Span* PopFront()
	{
		Span* front = _head->_next;
		Erase(front);
		return front;
	}
private:
	Span* _head = nullptr;
public:
	std::mutex _bucket_lock;//桶锁
};


