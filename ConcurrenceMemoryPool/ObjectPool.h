#pragma once
#include "Common.h"


//定长内存池
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		//先使用已经释放的内存块
		if (_free_list != nullptr)
		{
			void* next = *((void**)_free_list);
			obj = (T*)_free_list;
			_free_list = next;
		}
		else
		{
			//剩余内存不够一个对象大小时，重新开空间
			if (_remain_bytes < sizeof(T))
			{
				_remain_bytes = 128 * 1024;
				//_memory = (char*)malloc(_remain_bytes);
				_memory = (char*)SystemAlloc(_remain_bytes >> 13);
				if (_memory == nullptr)
				{
					printf("malloc fail");
					exit(-1);
				}
			}
			
			obj = (T*)_memory;
			size_t obj_size = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);//保证对象能够存下指针的大小 比指针还小就多开点
			_memory += obj_size;//char* 用了多少直接加
			_remain_bytes -= obj_size;

		}
		//定位new在已分配的内存块上使用定位 new 构造对象
		new(obj)T;
		return obj;
	}
	void Delete(T* obj)
	{
		//显示调用该对象的析构函数清理该对象
		obj->~T();
		//自由链表头插
		*(void**)obj = _free_list;
		_free_list = obj;
	}

private:
	char* _memory = nullptr;//维护大的内存池指针
	size_t _remain_bytes = 0;//剩余内存字节数
	void* _free_list = nullptr;//管理释放回来的自由链表头指针
};

//struct TreeNode
//{
//	int _val;
//	TreeNode* _left;
//	TreeNode* _right;
//
//	TreeNode()
//		:_val(0)
//		, _left(nullptr)
//		, _right(nullptr)
//	{}
//};

//void test()
//{
//	// 申请释放的轮次
//	const size_t Rounds = 5;
//
//	// 每轮申请释放多少次
//	const size_t N = 10000000;
//
//	//使用new和delete
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//	size_t begin1 = clock();
//	for (int i = 0; i < Rounds; ++i)
//	{
//		for (int j = 0; j < N; ++j)
//		{
//			v1.push_back(new TreeNode());
//		}
//		for (int j = 0; j < N; ++j)
//		{
//			delete v1[j];
//		}	
//		v1.clear();
//	}
//	size_t end1 = clock();
//	//使用内存池
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//	ObjectPool<TreeNode> obj_pool;
//	size_t begin2 = clock();
//	for (int i = 0; i < Rounds; ++i)
//	{
//		for (int j = 0; j < N; ++j)
//		{
//			v2.push_back(obj_pool.New());
//		}
//		for (int j = 0; j < N; ++j)
//		{
//			obj_pool.Delete(v2[j]);
//		}
//		v2.clear();
//	}
//	size_t end2 = clock();
//
//	cout << "USE NEW TIME = :" << end1 - begin1 << endl;
//	cout << "USE OBJECTPOOL TIME = :" << end2 - begin2 << endl;
//}