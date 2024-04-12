#pragma once
#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
//线程申请空间入口
//并发分配内存
void* ConcurrentAlloc(size_t size)
{
	if (size > MAX_BYTES)	//找page cache申请
	{	
		//确定对齐规则
		size_t align_size = SizeClass::RoundUp(size);
		//根据对齐规则的字节数 计算对应的页数 小于128页的都能使用内存池申请 大于128的让page cache直接向堆区申请即可
		size_t page_num = align_size >> PAGE_SHIFT;
		//cout << "page_num = " << page_num << endl;
		PageCache::GetInstance()->_page_mtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(page_num);
		span->_obj_size = align_size;
		PageCache::GetInstance()->_page_mtx.unlock();
		//根据span的页号 算出对应的地址
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
//并发释放 优化后不用传对象大小了
void ConcurrentFree(void* ptr)
{
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_obj_size;
	//cout << "释放对象大小为:" << size << endl;
	if (size > MAX_BYTES)//大于256kb 还给page cache即可
	{
		//根据哈希映射 确定地址属于哪一个span
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
