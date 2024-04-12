#pragma once
#include "Common.h"


//�����ڴ��
template<class T>
class ObjectPool
{
public:
	T* New()
	{
		T* obj = nullptr;
		//��ʹ���Ѿ��ͷŵ��ڴ��
		if (_free_list != nullptr)
		{
			void* next = *((void**)_free_list);
			obj = (T*)_free_list;
			_free_list = next;
		}
		else
		{
			//ʣ���ڴ治��һ�������Сʱ�����¿��ռ�
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
			size_t obj_size = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);//��֤�����ܹ�����ָ��Ĵ�С ��ָ�뻹С�Ͷ࿪��
			_memory += obj_size;//char* ���˶���ֱ�Ӽ�
			_remain_bytes -= obj_size;

		}
		//��λnew���ѷ�����ڴ����ʹ�ö�λ new �������
		new(obj)T;
		return obj;
	}
	void Delete(T* obj)
	{
		//��ʾ���øö����������������ö���
		obj->~T();
		//��������ͷ��
		*(void**)obj = _free_list;
		_free_list = obj;
	}

private:
	char* _memory = nullptr;//ά������ڴ��ָ��
	size_t _remain_bytes = 0;//ʣ���ڴ��ֽ���
	void* _free_list = nullptr;//�����ͷŻ�������������ͷָ��
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
//	// �����ͷŵ��ִ�
//	const size_t Rounds = 5;
//
//	// ÿ�������ͷŶ��ٴ�
//	const size_t N = 10000000;
//
//	//ʹ��new��delete
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
//	//ʹ���ڴ��
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