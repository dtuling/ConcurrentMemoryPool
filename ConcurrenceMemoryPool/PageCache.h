#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.hpp"
//单例
class PageCache
{
public:
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache) = delete;

	static PageCache* GetInstance()
	{
		return &_inst;
	}
	//获取size页的Span给中心缓存
	Span* NewSpan(size_t size);

	// 获取从对象到span的映射
	Span* MapObjectToSpan(void* obj);

	// 释放空闲span回到Pagecache，并合并相邻的span
	void ReleaseSpanToPageCache(Span* span);
	std::mutex _page_mtx;
private:
	PageCache() {}
	static PageCache _inst;//只是声明
	SpanList _span_lsit[PAGE_NUMS];
	//std::unordered_map<PAGE_ID, Span*> _id_span_map;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _id_span_map;
	ObjectPool<Span> _span_pool;
};