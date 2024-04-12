#pragma once
#include "Common.h"
//单例
class CentralCache
{
public:
	static CentralCache* GetInstace()
	{
		return &_inst;
	}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache) = delete;

	// 从中心缓存获取一定数量的对象给thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t num, size_t size);
	// 获取一个非空的span
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	// 将一定数量的对象释放到span跨度
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	CentralCache()
	{}
	SpanList _span_lists[NFREELISTS]; // 按对齐方式映射
	static CentralCache _inst;//静态成员变量 一定要在源文件中定义 这里只是声明
};
