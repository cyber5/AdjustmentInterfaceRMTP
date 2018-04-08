#pragma once

#include "Queue.h"
#include "Message.h"
#include "UBMessage.h"
#include <string>
#include <ctime>

template <class T>
class Sender
{
private:
	int delta_min;
	int delta_max;
	unsigned int counterA;
	unsigned int AuxA;
	
	std::queue<UBMessage<T>> send_pending;

	Queue<T>* to_sender;
	Queue<UBMessage<T>>* to_channel;
	Queue<UBMessage<T>>* back_to_sender;
	HANDLE done;

	//Adjustment Interface variables
	int numTags;
	int gamma;

public:
	Sender(int _delta_min, int _delta_max, Queue<T>* _to_sender, Queue<UBMessage<T>>* _to_channel, Queue<UBMessage<T>>* _back_to_sender, HANDLE _done)
	{
		delta_min = _delta_min;
		delta_max = _delta_max;
		gamma = delta_min;
		numTags = gamma + delta_max;
		counterA = 0;
		AuxA = 0;

		send_pending = std::queue<UBMessage<T>>();

		to_sender = _to_sender;
		to_channel = _to_channel;
		back_to_sender = _back_to_sender;
		done = _done;
	}

	void SEND()
	{
		//we already checked if empty
		T message = to_sender->dequeue();

		//enqueue the message
		send_pending.push(UBMessage<T>(message, counterA, numTags, AuxA));

		//increment the bounded counter
		counterA = (counterA + 1) % numTags;

		//increment the unbounded counter;
		++AuxA;
	}

	void send()
	{
		//we already checked if empty
		UBMessage<T> message = send_pending.front();
		send_pending.pop();

		//send to the channel
		to_channel->enqueue(message);
	}

	void receive_back()
	{
		//we already checked if empty
		UBMessage<T> message = back_to_sender->dequeue();

		//this message is the "BAD" message in the Adjustment Interface
		//it contains information about a minimum value of delta
		//so we recalculate numTags using the max value of delta plus this new minimum as gamma
		unsigned int new_min_delta = message.get_counter();

		if(new_min_delta > gamma)
		{
			//need to adjust
			gamma = new_min_delta;
			numTags = gamma + delta_max;

			counterA = 0;
			//Need to update the tags and numTags of every message in send_pending
			int spSize = send_pending.size();
			for (int i = 0; i < spSize; i++)
			{
				UBMessage<T> tempMsg = send_pending.front();
				send_pending.pop();

				tempMsg.changeCounterAndNumTags(counterA, numTags);

				send_pending.push(tempMsg);

				counterA = (counterA + 1) % numTags;
			}
		}
	}

	bool enabledSEND()
	{
		return !(to_sender->isEmpty());
	}

	bool enabledsend()
	{
		return !(send_pending.empty());
	}

	bool enabledreceive_back()
	{
		return !(back_to_sender->isEmpty());
	}

	HANDLE getDone()
	{
		return done;
	}
};




DWORD WINAPI senderThread(LPVOID lpParam)
{
	printf("start sender thread\n");

	Sender<std::string> *senderPtr = (Sender<std::string>*) lpParam;

	//choose a seed based on the time
	srand(clock());

	//loop:
	while (true)
	{
		//check to see if we're done
		if (WaitForSingleObject(senderPtr->getDone(), 1) == WAIT_OBJECT_0)
		{
			break;
		}

		//not done, so carry on
		//always receive_back if we can, because this event is begun in the channel
		if (senderPtr->enabledreceive_back())
		{
			senderPtr->receive_back();
		}

		//randomly determine the event that will take place
		bool enabledSEND = senderPtr->enabledSEND();
		bool enabledsend = senderPtr->enabledsend();
		int r = rand() % 2;

		switch (r)
		{
		//SEND(m)
		case 0:
			if (enabledSEND)
			{
				senderPtr->SEND();
			}
			break;
		//send(m)
		case 1:
			if (enabledsend)
			{
				senderPtr->send();
			}
			break;
		//no default cases
		default:
			break;
		}
	}
	
	printf("end sender thread\n");
	return 0;
}