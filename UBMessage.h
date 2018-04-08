#pragma once

#include "Message.h"

template <class T>
class UBMessage : public Message<T>
{
protected:
	unsigned int UB_counter;

public:
	UBMessage(T _contents, unsigned int _counter, unsigned int _numTags, unsigned int _UB_counter) : Message(_contents, _counter, _numTags)
	{
		UB_counter = _UB_counter;
	}

	unsigned int get_UB_counter() { return UB_counter; }
};