#pragma once
#include "Common.h"
//����
class CentralCache
{
public:
	static CentralCache* GetInstace()
	{
		return &_inst;
	}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache) = delete;

	// �����Ļ����ȡһ�������Ķ����thread cache
	size_t FetchRangeObj(void*& start, void*& end, size_t num, size_t size);
	// ��ȡһ���ǿյ�span
	Span* GetOneSpan(SpanList& list, size_t byte_size);
	// ��һ�������Ķ����ͷŵ�span���
	void ReleaseListToSpans(void* start, size_t byte_size);
private:
	CentralCache()
	{}
	SpanList _span_lists[NFREELISTS]; // �����뷽ʽӳ��
	static CentralCache _inst;//��̬��Ա���� һ��Ҫ��Դ�ļ��ж��� ����ֻ������
};
