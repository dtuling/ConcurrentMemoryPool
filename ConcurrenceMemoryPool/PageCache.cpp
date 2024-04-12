#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

PageCache PageCache::_inst;

//获取一个size页的span
Span* PageCache::NewSpan(const size_t size)
{
	if (size > PAGE_NUMS -1)//大于128页 
	{
		void* ptr = SystemAlloc(size);
		//申请的内存交给span管理
		Span* span = _span_pool.New();
		//根据地址确定页号
		span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_page_num = size;
		//_id_span_map[span->_page_id] = span;
		//_id_span_map[span->_page_id + span->_page_num - 1] = span;
		_id_span_map.set(span->_page_id,span);
		return span;
	}
	//SpanList中有就返回 没有就从堆中申请 
	// 一页8字节 128页 128*8*1024字节 一百万字节
	if (!_span_lsit[size].Empty())
	{
		//建立id和span的映射关系
		Span* kspan = _span_lsit[size].PopFront();
		for (PAGE_ID i = 0; i < kspan->_page_num; ++i)
		{
			//_id_span_map[kspan->_page_id + i] = kspan;
			_id_span_map.set(kspan->_page_id, kspan);
		}
		return kspan;
	}

	//检查下一个桶有没有
	for (int i = size + 1; i < PAGE_NUMS; ++i)
	{
		if (!_span_lsit[i].Empty())
		{
			//切分
			Span* nspan = _span_lsit[i].PopFront();
			Span* kspan = _span_pool.New();

			kspan->_page_id = nspan->_page_id;
			kspan->_page_num = size;

			nspan->_page_id += size;
			nspan->_page_num -= size;
			//将切分剩余的页头插到对应的桶下面
			_span_lsit[nspan->_page_num].PushFront(nspan);
			//建立nspan收尾页号的映射关系 方便page cache回收内存时进行合并查找
			//_id_span_map[nspan->_page_id] = nspan;
			//_id_span_map[nspan->_page_id + nspan->_page_num - 1] = nspan;
			_id_span_map.set(nspan->_page_id, nspan);
			//建立id和span的映射关系
			for (PAGE_ID i = 0; i < kspan->_page_num; ++i)
			{
				//_id_span_map[kspan->_page_id + i] = kspan;
				_id_span_map.set(kspan->_page_id, kspan);
			}

			/*if (kspan->_page_num != size)
			{
				int x = 0;
			}
			else
			{
				cout << " yes" << endl;
			}*/
			return kspan;
		}
	}

	//从堆区申请
	Span* span = _span_pool.New();
	void* ptr = SystemAlloc(PAGE_NUMS - 1);
	//cout << "从堆区申请1次" << endl;
	//根据地址确定页号  地址除8*1024即可(一页8kb)
	span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
	span->_page_num = PAGE_NUMS - 1;
	//插入到128号桶下
	_span_lsit[span->_page_num].PushFront(span);
	//切分 递归调用自己进行切分即可，不用再写切分逻辑了
	return NewSpan(size);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	//std::unique_lock<std::mutex> lock(_page_mtx);
	//根据地址获取页号
	PAGE_ID id = ((PAGE_ID)obj >> PAGE_SHIFT);

	Span* ret = (Span*)_id_span_map.get(id);
	assert(ret != nullptr);
	return ret;
	/*auto ret = _id_span_map.find(id);
	if (ret != _id_span_map.end())
	{
		return ret->second;
	}
	else
	{
		return nullptr;
	}	*/
}

void PageCache::ReleaseSpanToPageCache(Span* span)
{
	size_t size = span->_page_num;
	if (size > PAGE_NUMS - 1)
	{
		//根据页号计算地址
		void* ptr = (void*)(span->_page_id << PAGE_SHIFT);
		cout << "free ptr = " << ptr << endl;
		SystemFree(ptr);
		_span_pool.Delete(span);
		return;
	}
	//向前合并
	while (1)
	{
		PAGE_ID prev_span_page_id = span->_page_id - 1;
		/*auto ret = _id_span_map.find(prev_span_page_id);
		if (ret == _id_span_map.end())*/
		Span* ret = (Span*)_id_span_map.get(prev_span_page_id);
		if(ret == nullptr)
		{
			break;
		}
		Span* prev_span = ret;
		if (prev_span->_is_use)
		{
			break;
		}
		if (prev_span->_page_num + span->_page_num > PAGE_NUMS - 1)
		{
			break;
		}
		//合并
		span->_page_num += prev_span->_page_num;
		span->_page_id = prev_span->_page_id;
		_span_lsit[prev_span->_page_num].Erase(prev_span);
		_span_pool.Delete(prev_span);
	}
	//向后合并
	while (1)
	{
		PAGE_ID next_span_page_id = span->_page_id + span->_page_num;
		/*auto ret = _id_span_map.find(next_span_page_id);
		if (ret == _id_span_map.end())*/

		Span* ret = (Span*)_id_span_map.get(next_span_page_id);
		{
			break;
		}
		Span* next_span = ret;
		if (next_span->_is_use)
		{
			break;
		}
		if (span->_page_num + next_span->_page_num > PAGE_NUMS - 1)
		{
			break;
		}
		//合并
		span->_page_num += next_span->_page_num;
		_span_lsit[next_span->_page_num].Erase(next_span);
		_span_pool.Delete(span);
	}

	//将合并好的插入到对应的页号下
	_span_lsit[span->_page_num].PushFront(span);
	span->_is_use = false;
	//更新页的首尾映射关系
	//_id_span_map[span->_page_id] = span;
	//_id_span_map[span->_page_id + span->_page_num - 1] = span;

	_id_span_map.set(span->_page_id, span);
	_id_span_map.set(span->_page_id + span->_page_num - 1, span);
}
