#include "CentralCache.h"
#include "PageCache.h"
//һ����Դ�ļ��ж��� static ��Ա���� 
CentralCache CentralCache::_inst;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batch_num, size_t size)
{
	//���Ļ����thread cache�Ĺ�ϣ�ṹ��һ����ʹ��ͬ���ķ�ʽȷ���±�
	const size_t index = SizeClass::Index(size);
	//��Ͱ��
	_span_lists[index]._bucket_lock.lock();
	//�ӹ�ϣͰ�л�ȡspan����(���Ǽ�����)	
	Span* span = GetOneSpan(_span_lists[index], size);
	assert(span != nullptr);
	assert(span->_free_list != nullptr);

	//��Span�л�ȡnum���ڴ��
	//span�п��ܲ���num�� �ж���������� 
	//span���ܺܶ� ���Ҫbatch_num��(thread cacheһ����������ĸ���)
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
	//����span�е�free_list ��use_count
	span->_free_list = NextObj(end);
	NextObj(end) = nullptr; // ȡ����һ������ı�β�ÿ�
	span->_use_count += actual_num;

	_span_lists[index]._bucket_lock.unlock();	

	return actual_num;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//����SpanList ����δʹ�õ�Span
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
	//��pagecache�л�ȡ�ڴ�ʱ�Ƚ����������߳��ͷ��ڴ��ʱ����Ӱ��
	list._bucket_lock.unlock(); 

	//û��Span���� ȥPage Cache������  ��page cache������ʱҪ������
	PageCache::GetInstance()->_page_mtx.lock();
	Span* span = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	span->_is_use = true;
	span->_obj_size = size;
	PageCache::GetInstance()->_page_mtx.unlock();

	//�зֵ�ʱ���ü���  
	//��span�����з� ��Ҫ֪������ڴ����ʼ��ַ�ʹ���ڴ���ֽ���
	char* start = (char*)(span->_page_id << PAGE_SHIFT);//ҳ��*8kb������ʼ��ַ
	size_t byte_size = span->_page_num << PAGE_SHIFT;//ҳ������*8kb�����ֽ���
	char* end = start + byte_size;
	//������ڴ��зֳ�С���ڴ�ҵ�������������
	//β�� ͷ��Ļ���ַ������ �Ƿ���
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

	//���кõ�span���뵽spanlist�� (Ҫ��Ͱ��) �ָ��� FetchRangeObj �����
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
		//���ڴ��ҵ���Ӧ��span������ ��Ҫ֪���ڴ���span��ӳ���ϵ
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
		void* next = NextObj(start);
		//ͷ�嵽sapn�µ�freelist��
		NextObj(start) = span->_free_list;
		span->_free_list = start;
		span->_use_count--;

		//sapn�µ�����С���ڴ涼������ ����pagecache ���кϲ� ����ڴ���Ƭ����
		if (span->_use_count == 0)
		{
			_span_lists[index].Erase(span);
			span->_free_list = nullptr;
			span->_next = span->_prev = nullptr;

			_span_lists[index]._bucket_lock.unlock();

			PageCache::GetInstance()->_page_mtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_page_mtx.unlock();

			_span_lists[index]._bucket_lock.lock();//�ָ���
		}
		start = next;
	}

	_span_lists[index]._bucket_lock.unlock();

}
