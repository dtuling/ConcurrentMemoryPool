#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
//�߳�����ռ����
//���������ڴ�
void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)	//��page cache����
	{	
		//ȷ���������
		size_t align_size = SizeClass::RoundUp(size);
		//���ݶ��������ֽ��� �����Ӧ��ҳ�� С��128ҳ�Ķ���ʹ���ڴ������ ����128����page cacheֱ����������뼴��
		size_t page_num = align_size >> PAGE_SHIFT;
		//cout << "page_num = " << page_num << endl;
		PageCache::GetInstance()->_page_mtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(page_num);
		span->_obj_size = align_size;
		PageCache::GetInstance()->_page_mtx.unlock();
		//����span��ҳ�� �����Ӧ�ĵ�ַ
		void* ptr = (void*)(span->_page_id << PAGE_SHIFT);
		return ptr;
	}
	else
	{

		if (tls_threadcache == nullptr)
		{
			static ObjectPool<ThreadCache> thread_cache_pool;
			//tls_threadcache = new ThreadCache();
			tls_threadcache = thread_cache_pool.New();
		}
		return tls_threadcache->Allocate(size);
	}
}
//�����ͷ� �Ż����ô������С��
void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_obj_size;
	//cout << "�ͷŶ����СΪ:" << size << endl;
	if (size > MAX_BYTES)//����256kb ����page cache����
	{
		//���ݹ�ϣӳ�� ȷ����ַ������һ��span
		PageCache::GetInstance()->_page_mtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_page_mtx.unlock();
	}
	else
	{
		assert(tls_threadcache);
		tls_threadcache->Deallocate(ptr, size);
	}
	
}
