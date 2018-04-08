#pragma once

#include <queue>
#include <windows.h>

template <class T>
class Queue
{
private:
	std::queue<T> q;
	CRITICAL_SECTION cs;

public:
	Queue()
	{
		q = std::queue<T>();
		cs = CRITICAL_SECTION();
		InitializeCriticalSection(&cs);
	}

	void enqueue(T t)
	{
		EnterCriticalSection(&cs);
			q.push(t);
		LeaveCriticalSection(&cs);
	}

	T dequeue()
	{
		EnterCriticalSection(&cs);
			T val = q.front();
			q.pop();
		LeaveCriticalSection(&cs);
		return val;
	}

	bool isEmpty()
	{
		bool ret;
		EnterCriticalSection(&cs);
			ret = q.empty();
		LeaveCriticalSection(&cs);
		return ret;
	}

	int size()
	{
		int sz;
		EnterCriticalSection(&cs);
			sz = q.size();
		LeaveCriticalSection(&cs);
		return sz;
	}
};