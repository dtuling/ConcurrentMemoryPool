#define _CRT_SECURE_NO_WARNINGS 1

#include "ThreadCache.h"
#include "CentralCache.h"

//С���� ������� ����� �ٸ�����
void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	// ����ʼ���������㷨
	//NumMoveSize��������	
	size_t batch_num = min(_free_list[index].MaxSize(),SizeClass::NumMoveSize(size));
	if (batch_num == _free_list[index].MaxSize())
	{
		_free_list[index].MaxSize()+=1;
	}

	// �����Ļ����л�ȡһ���ڴ� ���ܻ������ ������ķŵ�thread cache����
	void* start = nullptr;
	void* end = nullptr;
	size_t actual_num = CentralCache::GetInstace()->FetchRangeObj(start, end, batch_num, size);
	assert(actual_num >= 1);//��������һ��
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
	//ȡһ���ڴ� ��Central Cache
	void* start = nullptr;
	void* end = nullptr;
	list.PopRound(start, end, list.MaxSize());
	CentralCache::GetInstace()->ReleaseListToSpans(start, size);
}


//������� ������������Pop����
void* ThreadCache::Allocate(size_t size)
{
	assert(size <= MAX_BYTES);
	//ȷ���������
	size_t align_size = SizeClass::RoundUp(size);
	//ȷ��Ͱ��λ��
	size_t index = SizeClass::Index(align_size);
	//eg ����127�ֽ�  ����8�ֽڶ��룬ʵ��align_size = 128, index = 15��
	if (_free_list[index].IsEmpty())//�վ�ȥ���Ļ����ȡ ��ʱ�����Ǵ���256kb�����
	{
		//���Ļ���һ�ο��ܻ������ռ䣬ֻ���ص�ǰ�������Ĵ�С��ʣ��Ĵ浽thread cache������������
		return FetchFromCentralCache(index, align_size);
	}
	else
	{
		return _free_list[index].Pop();
	}
}

void ThreadCache::Deallocate(void* ptr, size_t size)
{
	//���ݴ�Сȷ��Ͱ��λ��
	size_t index = SizeClass::Index(size);
	_free_list[index].Push(ptr);
	//������������������Ĵ�С  ����Cetral Cache
	if (_free_list[index].Size() >= _free_list[index].MaxSize())
	{
		ListTooLong(_free_list[index], size);
	}
	
}


//�ͷŶ���push������������
//void ThreadCache::Deallocate(void* ptr, size_t size)
//{
//	//���ݴ�Сȷ��λ��push
//	size_t index = SizeClass::Index(size);
//	_free_list[index].Push(ptr);
//}
