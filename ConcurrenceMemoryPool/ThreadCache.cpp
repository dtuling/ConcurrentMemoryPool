#define _CRT_SECURE_NO_WARNINGS 1

#include "ThreadCache.h"
#include "CentralCache.h"

//小对象 多给几个 大对象 少给几个
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// 慢开始反馈调节算法
	//NumMoveSize决定上限	
	size_t batch_num = min(_free_list[index].MaxSize(),SizeClass::NumMoveSize(size));
	if (batch_num == _free_list[index].MaxSize())
	{
		_free_list[index].MaxSize()+=1;
	}

	// 从中心缓存中获取一段内存 可能会多申请 多申请的放到thread cache即可
	void* start = nullptr;
	void* end = nullptr;
	size_t actual_num = CentralCache::GetInstace()->FetchRangeObj(start, end, batch_num, size);
	assert(actual_num >= 1);//至少申请一个
	if (actual_num == 1)
	{
		return start;
	}
	else
	{
		_free_list[index].PushRound(NextObj(start), end, actual_num -1 );
		return start;
	}

}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	//取一段内存 给Central Cache
	void* start = nullptr;
	void* end = nullptr;
	list.PopRound(start, end, list.MaxSize());
	CentralCache::GetInstace()->ReleaseListToSpans(start, size);
}


//申请对象 从自由链表中Pop即可
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	//确定对齐规则
	size_t align_size = SizeClass::RoundUp(size);
	//确定桶的位置
	size_t index = SizeClass::Index(align_size);
	//eg 申请127字节  按照8字节对齐，实际align_size = 128, index = 15。
	if (_free_list[index].IsEmpty())//空就去中心缓存获取 暂时不考虑大于256kb的情况
	{
		//中心缓存一次可能会多申请空间，只返回当前对齐数的大小，剩余的存到thread cache的自由链表中
		return FetchFromCentralCache(index, align_size);
	}
	else
	{
		return _free_list[index].Pop();
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	//根据大小确定桶的位置
	size_t index = SizeClass::Index(size);
	_free_list[index].Push(ptr);
	//自由链表超过批量申请的大小  还给Cetral Cache
	if (_free_list[index].Size() >= _free_list[index].MaxSize())
	{
		ListTooLong(_free_list[index], size);
	}
	
}


//释放对象，push到自由链表中
//void ThreadCache::Deallocate(void* ptr, size_t size)
//{
//	//根据大小确定位置push
//	size_t index = SizeClass::Index(size);
//	_free_list[index].Push(ptr);
//}
