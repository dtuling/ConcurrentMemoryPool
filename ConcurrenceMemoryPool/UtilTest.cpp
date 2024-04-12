//
//#include "ConcurrentAlloc.h"
//#include <thread>
//#include <string>
//
//void TestConcurrentAlloc1()
//{
//	void* p1 = ConcurrentAlloc(6);
//	void* p2 = ConcurrentAlloc(8);
//	void* p3 = ConcurrentAlloc(1);
//	void* p4 = ConcurrentAlloc(7);
//	void* p5 = ConcurrentAlloc(8);
//	void* p6 = ConcurrentAlloc(8);
//	void* p7 = ConcurrentAlloc(8);
//
//
//	cout << p1 << endl;
//	cout << p2 << endl;
//	cout << p3 << endl;
//	cout << p4 << endl;
//	cout << p5 << endl;
//
//	ConcurrentFree(p1);
//	ConcurrentFree(p2);
//	ConcurrentFree(p3);
//	ConcurrentFree(p4);
//	ConcurrentFree(p5);
//	ConcurrentFree(p6);
//	ConcurrentFree(p7);
//}
//void TestConcurrentAlloc2()
//{
//	for (size_t i = 0; i < 1024; ++i)
//	{
//		void* p1 = ConcurrentAlloc(6);
//		cout << p1 << endl;
//	}
//
//	void* p2 = ConcurrentAlloc(8);
//	cout << p2 << endl;
//}
//
//void MultiThreadAlloc1()
//{
//	std::vector<void*> v;
//	for (size_t i = 0; i < 1024; ++i)
//	{
//		void* ptr = ConcurrentAlloc(6);
//		v.push_back(ptr);
//	}
//
//	for (auto e : v)
//	{
//		ConcurrentFree(e);
//	}
//}
//
//void MultiThreadAlloc2()
//{
//	std::vector<void*> v;
//	for (size_t i = 0; i < 1024; ++i)
//	{
//		void* ptr = ConcurrentAlloc(16);
//		v.push_back(ptr);
//	}
//
//	for (auto e : v)
//	{
//		ConcurrentFree(e);
//	}
//}
//void TestMultiThread()
//{
//	std::thread t1(MultiThreadAlloc1);
//	std::thread t2(MultiThreadAlloc2);
//
//	t1.join();
//	t2.join();
//}
//
////int main()
////{
////	//TestConcurrentAlloc2();
////	TestMultiThread();
////}