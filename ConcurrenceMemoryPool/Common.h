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
static const size_t PAGE_NUMS = 129;//ҳ��
static const size_t PAGE_SHIFT = 13;//һҳ8kb  2^13


#ifdef _WIN32
#include <Windows.h>
#else
//...
#endif

//ֱ��ȥ�������밴ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
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
	// sbrk unmmap��
#endif
}


static void* &NextObj(void* obj)
{
	return *(void**)obj;
}

//��������
class FreeList
{
public:
	//ͷ��
	void Push(void* obj)
	{
		*(void**)obj = _free_list;
		_free_list = obj;
		++_size;
	}
	//ͷ��һ������
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
	//ͷɾ
	void*Pop()
	{
		void* next = *((void**)_free_list);
		void* obj = _free_list;
		_free_list = next;
		--_size;
		return obj;
	}
	//ͷɾһ������
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
	//��ȡ��ǰ����������
	size_t& MaxSize()
	{
		return _max_size;
	}
	//��ȡ��ǰ����Ԫ�ظ���
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
	// ������1%-12%���ҵ�����Ƭ�˷�
	// [1,128]					8byte����	     freelist[0,16)
	// [129,1024]				16byte����		 freelist[16,72)
	// [1025,8*1024]			128byte����	     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����     freelist[128,184)
	// [64*1024+1,256*1024]     8*1024byte����   freeist[184,208)
	// 
	//static inline size_t _RoundUp(size_t bytes, size_t align)
	//{
	//	size_t align_num = 0;
	//	if (bytes % align == 0)//��������
	//	{
	//		align_num = bytes;
	//	}
	//	else //����7�ֽڣ���8�ֽ�
	//	{
	//		align_num = ((bytes / align) + 1) * align;
	//	}
	//	return align_num;
	//}
	// 
	// tcmalloc���д�� ������
	static inline size_t _RoundUp(size_t bytes, size_t align)
	{
		return ((bytes + align - 1) & ~(align - 1));
	}
	//���ݶ����С����������   
	static inline size_t RoundUp(size_t bytes)
	{
		if (bytes <= 128)
		{
			return _RoundUp(bytes, 8);//����8�ֽڶ���
		}
		else if (bytes <= 1024)
		{
			return _RoundUp(bytes, 16);//����16�ֽڶ���
		}
		else if (bytes <= 8 * 1024)
		{
			return _RoundUp(bytes, 128);//����128�ֽڶ���
		}
		else if (bytes <= 64 * 1024)
		{
			return _RoundUp(bytes, 1024);//����1024�ֽڶ���
		}
		else if (bytes <= 256 * 1024)
		{
			return _RoundUp(bytes, 8*1024);//����8*1024�ֽڶ���
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
	
	// [1,128]					8byte����	     freelist[0,16)
	// [129,1024]				16byte����		 freelist[16,72)
	// [1025,8*1024]			128byte����	     freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����     freelist[128,184)
	// [64*1024+1,256*1024]     8*1024byte����   freeist[184,208)
	//ȷ��Ͱ��λ�� 
	//���п��д��
	static inline size_t _Index(size_t bytes, size_t alignShift)
	{
		return ((bytes + (1 << alignShift) - 1) >> alignShift) - 1;
	}
	//ӳ�䵽���������λ��
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);
		//ÿ�����ж��ٸ�Ͱ
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


	// һ��thread cache�����Ļ����ȡ���ٸ�
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		// [2, 512]��һ�������ƶ����ٸ������(������)����ֵ
		// С����һ���������޸�
		// С����һ���������޵�
		int num = MAX_BYTES / size;
		if (num < 2)
			num = 2;

		if (num > 512)
			num = 512;

		return num;
	}

	// ����һ����ϵͳ��ȡ����ҳ
	// �������� 8byte
	// ...
	// �������� 256KB
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
	PAGE_ID _page_id = 0; //ҳ��
	size_t _page_num = 0;//ҳ������

	Span* _prev = nullptr;
	Span* _next = nullptr;

	size_t _obj_size = 0;//�����С 
	size_t _use_count = 0;
	void* _free_list = nullptr;
	bool _is_use = false;//span�����Ƿ�ʹ��
};
//SpanList��һ����ͷ˫��ѭ������ ��sapnΪ�ڵ� ��ΪCentral�ĳ�Ա����
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
	std::mutex _bucket_lock;//Ͱ��
};


