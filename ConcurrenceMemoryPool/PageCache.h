#pragma once
#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.hpp"
//����
class PageCache
{
public:
	PageCache(const PageCache&) = delete;
	PageCache& operator=(const PageCache) = delete;

	static PageCache* GetInstance()
	{
		return &_inst;
	}
	//��ȡsizeҳ��Span�����Ļ���
	Span* NewSpan(size_t size);

	// ��ȡ�Ӷ���span��ӳ��
	Span* MapObjectToSpan(void* obj);

	// �ͷſ���span�ص�Pagecache�����ϲ����ڵ�span
	void ReleaseSpanToPageCache(Span* span);
	std::mutex _page_mtx;
private:
	PageCache() {}
	static PageCache _inst;//ֻ������
	SpanList _span_lsit[PAGE_NUMS];
	//std::unordered_map<PAGE_ID, Span*> _id_span_map;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _id_span_map;
	ObjectPool<Span> _span_pool;
};