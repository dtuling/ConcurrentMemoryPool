#pragma once
#include "Common.h"

class ThreadCache
{
public:
	//�����ڴ����
	void* Allocate(size_t size);
	//�ͷ��ڴ����
	void Deallocate(void* ptr,size_t size);
	// �����Ļ����ȡ����
	void* FetchFromCentralCache(size_t index, size_t size);
	// �ͷŶ���ʱ���������ʱ�������ڴ�ص����Ļ���
	void ListTooLong(FreeList& list, size_t size);
private:
	FreeList _free_list[NFREELISTS];//�����зֺõ���������(��ϣͰ)
};

//ʹ�� __declspec(thread) �����ı�����Ϊÿ���̴߳���һ�������ĸ���
//ÿ���̶߳�������������Ĵ洢�ռ��з��ʸñ�����������Ӱ�������̵߳ĸ�����
//�����ָ��ά��thread cache�����ڴ�ʱ������ͷ�
static __declspec(thread) ThreadCache* tls_threadcache = nullptr;