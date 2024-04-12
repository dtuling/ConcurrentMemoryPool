#include "CentralCache.h"
#include "PageCache.h"
//一定在源文件中定义 static 成员变量 
CentralCache CentralCache::_inst;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batch_num, size_t size)
{
	//中心缓存和thread cache的哈希结构是一样的使用同样的方式确定下标
	const size_t index = SizeClass::Index(size);
	//加桶锁
	_span_lists[index]._bucket_lock.lock();
	//从哈希桶中获取span对象(还是加锁的)	
	Span* span = GetOneSpan(_span_lists[index], size);
	assert(span != nullptr);
	assert(span->_free_list != nullptr);

	//从Span中获取num个内存块
	//span中可能不够num个 有多少申请多少 
	//span可能很多 最多要batch_num个(thread cache一次批量申请的个数)
	start = span->_free_list;
	end = start;
	size_t i = 0;
	size_t actual_num = 1;
	while (NextObj(end)!= nullptr  && (i < batch_num - 1))
	{
		end = NextObj(end);
		actual_num++;
		++i;
	}
	//更新span中的free_list 和use_count
	span->_free_list = NextObj(end);
	NextObj(end) = nullptr; // 取出的一段链表的表尾置空
	span->_use_count += actual_num;

	_span_lists[index]._bucket_lock.unlock();	

	return actual_num;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//遍历SpanList 查找未使用的Span
	Span* iter = list.Begin();
	while (iter != list.End())
	{
		if (iter->_free_list != nullptr)
		{
			return iter;
		}
		else
		{
			iter = iter->_next;
		}
	}
	//从pagecache中获取内存时先解锁，其他线程释放内存的时候不受影响
	list._bucket_lock.unlock(); 

	//没有Span对象 去Page Cache中申请  从page cache中申请时要加锁。
	PageCache::GetInstance()->_page_mtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_is_use = true;
	span->_obj_size = size;
	PageCache::GetInstance()->_page_mtx.unlock();

	//切分的时候不用加锁  
	//对span进行切分 需要知道大块内存的起始地址和大块内存的字节数
	char* start = (char*)(span->_page_id << PAGE_SHIFT);//页号*8kb就是起始地址
	size_t byte_size = span->_page_num << PAGE_SHIFT;//页的数量*8kb就是字节数
	char* end = start + byte_size;
	//将大块内存切分成小块内存挂到自由链表下面
	//尾插 头插的话地址不连续 是反的
	span->_free_list = start;
	start += size;
	void* tail = span->_free_list;
	while (start < end)
	{
		NextObj(tail) = start;
		tail = start;
		start += size;
	}
	NextObj(tail) = nullptr;

	//将切好的span插入到spanlist中 (要加桶锁) 恢复锁 FetchRangeObj 会解锁
	list._bucket_lock.lock();
	list.PushFront(span);
	return span;
}

void CentralCache::ReleaseListToSpans(void* start, const size_t size)
{
	const size_t index = SizeClass::Index(size);
	_span_lists[index]._bucket_lock.lock();
	while (start != nullptr)
	{
		//把内存块挂到对应的span对象中 需要知道内存块和span的映射关系
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		void* next = NextObj(start);
		//头插到sapn下的freelist中
		NextObj(start) = span->_free_list;
		span->_free_list = start;
		span->_use_count--;

		//sapn下的所有小块内存都回来了 还给pagecache 进行合并 解决内存碎片问题
		if (span->_use_count == 0)
		{
			_span_lists[index].Erase(span);
			span->_free_list = nullptr;
			span->_next = span->_prev = nullptr;

			_span_lists[index]._bucket_lock.unlock();

			PageCache::GetInstance()->_page_mtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_page_mtx.unlock();

			_span_lists[index]._bucket_lock.lock();//恢复锁
		}
		start = next;
	}

	_span_lists[index]._bucket_lock.unlock();

}
