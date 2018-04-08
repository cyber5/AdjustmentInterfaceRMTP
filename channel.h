#pragma once

#include "Queue.h"
#include "Message.h"
#include "UBMessage.h"
#include <vector>
#include <string>
#include <ctime>

//The chance that the channel will send the oldest unreceived message is at least 1/ONEINXODDS
#define ONEINXODDS 4

template <class T>
class Channel
{
private:
	int delta;

	std::vector<std::pair<UBMessage<T>, bool>> in_transit;
	std::vector<std::pair<UBMessage<T>, bool>> back_in_transit;

	Queue<UBMessage<T>>* to_channel;
	Queue<UBMessage<T>>* to_receiver;
	Queue<UBMessage<T>>* back_to_channel;
	Queue<UBMessage<T>>* back_to_sender;
	HANDLE done;

public:
	Channel(int _delta, Queue<UBMessage<T>>* _to_channel, Queue<UBMessage<T>>* _to_receiver, Queue<UBMessage<T>>* _back_to_channel, Queue<UBMessage<T>>* _back_to_sender, HANDLE _done)
	{
		delta = _delta;

		in_transit = std::vector<std::pair<UBMessage<T>, bool>>();
		back_in_transit = std::vector<std::pair<UBMessage<T>, bool>>();

		to_channel = _to_channel;
		to_receiver = _to_receiver;
		back_to_channel = _back_to_channel;
		back_to_sender = _back_to_sender;
		done = _done;
	}

	void send()
	{
		//we already checked if empty
		UBMessage<T> message = to_channel->dequeue();

		//insert the message
		in_transit.push_back(std::pair<UBMessage<T>, bool>(message, false));
	}

	void receive(int index)
	{
		//we already checked that this index is enabled
		
		//send to the receiver
		to_receiver->enqueue(in_transit[index].first);

		//set to TRUE
		in_transit[index].second = true;

		//remove messages that are too old
		if (index >= delta)
		{
			std::vector<std::pair<UBMessage<T>, bool>>::iterator iter = in_transit.begin();

			in_transit.erase(iter, iter + (index - delta + 1));
		}
	}

	void send_back()
	{
		//we already checked if empty
		UBMessage<T> message = back_to_channel->dequeue();

		//insert the message
		back_in_transit.push_back(std::pair<UBMessage<T>, bool>(message, false));
	}
	
	void receive_back(int index)
	{
		//we already checked that this index is enabled

		//send to the sender
		back_to_sender->enqueue(back_in_transit[index].first);

		//set to TRUE
		back_in_transit[index].second = true;

		//remove messages that are too old
		if (index >= delta)
		{
			std::vector<std::pair<UBMessage<T>, bool>>::iterator iter = back_in_transit.begin();

			back_in_transit.erase(iter, iter + (index - delta + 1));
		}
	}
	
	bool enabledsend()
	{
		return !(to_channel->isEmpty());
	}

	void enabledList(std::vector<int> &eList)
	{
		int trueCount = 0;

		//this loop serves two purposes
		//one purpose is to add the trivially-enabled message events to eList
		//another purpose is to keep track of the number of messages with bool value = TRUE in the range [0, size-delta)
		for (int i = 0; i < in_transit.size(); i++)
		{
			if (i < delta)
				eList.push_back(i);

			if ((in_transit[i].second) && (i < (in_transit.size() - delta)))
				++trueCount;
		}

		//this loop adds the non-trivially enabled events to eList
		for (int i = in_transit.size(); i > delta; i--)
		{
			if (trueCount == i - delta)
				eList.push_back(i - 1);

			if (in_transit[i - delta - 1].second)
				--trueCount;
		}
	}

	//returns index in in_transit of oldest unreceived message
	int oldestUnrecv()
	{
		for (int i = 0; i < in_transit.size(); i++)
		{
			if (!in_transit[i].second) //message in index i has not been received
			{
				return i;
			}
		}

		return -1; //all messages in in_transit have been received
	}

	bool enabledsend_back()
	{
		return !(back_to_channel->isEmpty());
	}

	void enabledListBack(std::vector<int> &eListBack)
	{
		int trueCount = 0;

		//this loop serves two purposes
		//one purpose is to add the trivially-enabled message events to eListBack
		//another purpose is to keep track of the number of messages with bool value = TRUE in the range [0, size-delta)
		for (int i = 0; i < back_in_transit.size(); i++)
		{
			if (i < delta)
				eListBack.push_back(i);

			if ((back_in_transit[i].second) && (i < (back_in_transit.size() - delta)))
				++trueCount;
		}

		//this loop adds the non-trivially enabled events to eListBack
		for (int i = back_in_transit.size(); i > delta; i--)
		{
			if (trueCount == i - delta)
				eListBack.push_back(i - 1);

			if (back_in_transit[i - delta - 1].second)
				--trueCount;
		}
	}

	//returns index in back_in_transit of oldest unreceived message
	int oldestUnrecvBack()
	{
		for (int i = 0; i < back_in_transit.size(); i++)
		{
			if (!back_in_transit[i].second) //message in index i has not been received
			{
				return i;
			}
		}

		return -1; //all messages in back_in_transit have been received
	}

	HANDLE getDone()
	{
		return done;
	}
};




DWORD WINAPI channelThread(LPVOID lpParam)
{
	printf("start channel thread\n");

	Channel<std::string> *channelPtr = (Channel<std::string>*) lpParam;

	//choose a seed based on the time
	srand(clock());

	//loop:
	while (true)
	{
		//check to see if we're done
		if (WaitForSingleObject(channelPtr->getDone(), 1) == WAIT_OBJECT_0)
		{
			break;
		}

		//not done, so carry on
		//always send when we can, because this event is begun in the Sender
		if (channelPtr->enabledsend())
		{
			channelPtr->send();
		}
		//always send_back when we can, because this event is begun in the Receiver
		if (channelPtr->enabledsend_back())
		{
			channelPtr->send_back();
		}

		//have at least a 1/ONEINXODDS chance of sending the oldest unsent message in in_transit
		int index = channelPtr->oldestUnrecv();
		if (index == -1 || rand() % ONEINXODDS != 0)
		{
			//randomly determine which message the Receiver will receive
			std::vector<int> eList;
			channelPtr->enabledList(eList);
			if (eList.size() > 0)
			{
				int r = rand() % eList.size();
				index = eList[r];
			}
		}
		if (index >= 0)
		{
			channelPtr->receive(index);
		}
		
		//have at least a 1/ONEINXODDS chance of sending the oldest unsent message in back_in_transit
		int indexBack = channelPtr->oldestUnrecvBack();
		if (indexBack == -1 || rand() % ONEINXODDS != 0)
		{
			//randomly determine which message the Sender will receive
			std::vector<int> eListBack;
			channelPtr->enabledListBack(eListBack);
			if (eListBack.size() > 0)
			{
				int rBack = rand() % eListBack.size();
				indexBack = eListBack[rBack];
			}
		}
		if (indexBack >= 0)
		{
			channelPtr->receive_back(indexBack);
		}
	}

	printf("end channel thread\n");
	return 0;
}