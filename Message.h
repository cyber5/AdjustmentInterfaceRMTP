#pragma once

//#include <string>

template <class T>
class Message
{
protected:
	T contents;
	unsigned int tag;
	unsigned int numTags; //use this so that the Receiver can differentiate between messages before and after adjustment

public:
	Message(T _contents, unsigned int _counter, unsigned int _numTags)
	{
		contents = _contents;
		tag = _counter;
		numTags = _numTags;
	}

	T get_contents() { return contents; }
	
	unsigned int get_counter() { return tag; }

	unsigned int get_numTags() { return numTags; }

	void changeCounterAndNumTags(unsigned int _counter, unsigned int _numTags) { tag = _counter; numTags = _numTags; }
};