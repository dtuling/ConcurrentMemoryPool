#define _CRT_SECURE_NO_WARNINGS 1
#include "PageCache.h"

PageCache PageCache::_inst;

//��ȡһ��sizeҳ��span
Span* PageCache::NewSpan(const size_t size)
{
	if (size > PAGE_NUMS -1)//����128ҳ 
	{
		void* ptr = SystemAlloc(size);
		//������ڴ潻��span����
		Span* span = _span_pool.New();
		//���ݵ�ַȷ��ҳ��
		span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
		span->_page_num = size;
		//_id_span_map[span->_page_id] = span;
		//_id_span_map[span->_page_id + span->_page_num - 1] = span;
		_id_span_map.set(span->_page_id,span);
		return span;
	}
	//SpanList���оͷ��� û�оʹӶ������� 
	// һҳ8�ֽ� 128ҳ 128*8*1024�ֽ� һ�����ֽ�
	if (!_span_lsit[size].Empty())
	{
		//����id��span��ӳ���ϵ
		Span* kspan = _span_lsit[size].PopFront();
		for (PAGE_ID i = 0; i < kspan->_page_num; ++i)
		{
			//_id_span_map[kspan->_page_id + i] = kspan;
			_id_span_map.set(kspan->_page_id, kspan);
		}
		return kspan;
	}

	//�����һ��Ͱ��û��
	for (int i = size + 1; i < PAGE_NUMS; ++i)
	{
		if (!_span_lsit[i].Empty())
		{
			//�з�
			Span* nspan = _span_lsit[i].PopFront();
			Span* kspan = _span_pool.New();

			kspan->_page_id = nspan->_page_id;
			kspan->_page_num = size;

			nspan->_page_id += size;
			nspan->_page_num -= size;
			//���з�ʣ���ҳͷ�嵽��Ӧ��Ͱ����
			_span_lsit[nspan->_page_num].PushFront(nspan);
			//����nspan��βҳ�ŵ�ӳ���ϵ ����page cache�����ڴ�ʱ���кϲ�����
			//_id_span_map[nspan->_page_id] = nspan;
			//_id_span_map[nspan->_page_id + nspan->_page_num - 1] = nspan;
			_id_span_map.set(nspan->_page_id, nspan);
			//����id��span��ӳ���ϵ
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

	//�Ӷ�������
	Span* span = _span_pool.New();
	void* ptr = SystemAlloc(PAGE_NUMS - 1);
	//cout << "�Ӷ�������1��" << endl;
	//���ݵ�ַȷ��ҳ��  ��ַ��8*1024����(һҳ8kb)
	span->_page_id = (PAGE_ID)ptr >> PAGE_SHIFT;
	span->_page_num = PAGE_NUMS - 1;
	//���뵽128��Ͱ��
	_span_lsit[span->_page_num].PushFront(span);
	//�з� �ݹ�����Լ������зּ��ɣ�������д�з��߼���
	return NewSpan(size);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	//std::unique_lock<std::mutex> lock(_page_mtx);
	//���ݵ�ַ��ȡҳ��
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
		//����ҳ�ż����ַ
		void* ptr = (void*)(span->_page_id << PAGE_SHIFT);
		cout << "free ptr = " << ptr << endl;
		SystemFree(ptr);
		_span_pool.Delete(span);
		return;
	}
	//��ǰ�ϲ�
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
		//�ϲ�
		span->_page_num += prev_span->_page_num;
		span->_page_id = prev_span->_page_id;
		_span_lsit[prev_span->_page_num].Erase(prev_span);
		_span_pool.Delete(prev_span);
	}
	//���ϲ�
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
		//�ϲ�
		span->_page_num += next_span->_page_num;
		_span_lsit[next_span->_page_num].Erase(next_span);
		_span_pool.Delete(span);
	}

	//���ϲ��õĲ��뵽��Ӧ��ҳ����
	_span_lsit[span->_page_num].PushFront(span);
	span->_is_use = false;
	//����ҳ����βӳ���ϵ
	//_id_span_map[span->_page_id] = span;
	//_id_span_map[span->_page_id + span->_page_num - 1] = span;

	_id_span_map.set(span->_page_id, span);
	_id_span_map.set(span->_page_id + span->_page_num - 1, span);
}
