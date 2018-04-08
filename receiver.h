#pragma once

#include "Queue.h"
#include "Message.h"
#include "UBMessage.h"
#include <list>
#include <map>
#include <vector>
#include <string>
#include <ctime>

template <class T>
class Receiver
{
private:
	int delta_min;
	int delta_max;
	unsigned int counterB;
	unsigned int AuxB;

	std::list<UBMessage<T>> receive_pending;
	std::list<UBMessage<T>> delivered;
	std::list<UBMessage<T>> lower_tagged;

	Queue<UBMessage<T>>* to_receiver;
	Queue<UBMessage<T>>* back_to_channel;
	Queue<UBMessage<T>>* to_final;
	HANDLE done;

	//Adjustment Interface variables
	unsigned int numTags; //number of tags currently in use
	int gamma; //estimate of the reordering bound delta
	bool sendBackNeeded;
	std::map<unsigned int, bool> expectedEnabled;

public:
	Receiver(int _delta_min, int _delta_max, Queue<UBMessage<T>>* _to_receiver, Queue<UBMessage<T>>* _to_final, Queue<UBMessage<T>>* _back_to_channel, HANDLE _done)
	{
		delta_min = _delta_min;
		delta_max = _delta_max;
		gamma = delta_min;
		numTags = gamma + delta_max;
		sendBackNeeded = false;
		counterB = 0;
		AuxB = 0;

		receive_pending = std::list<UBMessage<T>>();
		delivered = std::list<UBMessage<T>>();
		lower_tagged = std::list<UBMessage<T>>();

		to_receiver = _to_receiver;
		back_to_channel = _back_to_channel;
		to_final = _to_final;
		done = _done;
		
		expectedEnabled = std::map<unsigned int, bool>();
		//enable the first gamma tags
		for(unsigned int i = 0; i < gamma; i++)
		{
			expectedEnabled[i] = false;
		}
	}

	void receive()
	{
		//we already checked if empty
		UBMessage<T> message = to_receiver->dequeue();

		//printf("low-level message: tag = %u, numTags = %u, UB = %u\n", message.get_counter(), message.get_numTags(), message.get_UB_counter());

		//always be on the lookout for messages with old numTags values
		if (message.get_numTags() < numTags)
		{
			lower_tagged.push_back(message);
			return;
		}
		
		//run the Analyze() routine to see if this message's tag contradicts gamma
		bool tempTQ = Analyze(message.get_counter());
		
		if(!tempTQ)
		{
			//messages with this tag now need to be placed in lower_tagged
			lower_tagged.push_back(message);

			return;
		}

		AttemptAddToRP(message);
	}

	void RECEIVE()
	{
		//we already checked that this is enabled

		if (lower_tagged.size() > 0)
		{
			//RECEIVE an old, lower-numTags message
			UBMessage<T> lowMsg = lower_tagged.front();
			lower_tagged.pop_front();
			to_final->enqueue(lowMsg);

			return;
		}

		//remove from receive_pending
		UBMessage<T> message = receive_pending.front();
		receive_pending.pop_front();

		//give message to the final recipient
		to_final->enqueue(message);

		//increment the bounded counter
		counterB = (counterB + 1) % numTags;

		//increment the unbounded counter
		//TODO: remove the variable AuxB from the program
		++AuxB;

		//handle the delivered queue
		delivered.push_back(message);

		if (delivered.size() == gamma + 1)
		{
			delivered.pop_front();
		}
	}

	void send_back()
	{
		//gamma was already incremented in the "bad" route of Analyze()
		back_to_channel->enqueue(UBMessage<T>(T(), gamma, 0, 0));

		sendBackNeeded = false;
	}

	bool enabledreceive()
	{
		return !(to_receiver->isEmpty());
	}

	bool enabledRECEIVE()
	{
		//messages with old numTags values have priority
		if(lower_tagged.size() > 0)
		{
			return true;
		}

		if (receive_pending.empty())
		{
			return false;
		}
		
		UBMessage<T> frontMsg = receive_pending.front();
		int c = frontMsg.get_counter();

		return c == counterB;
	}

	bool enabledsend_back()
	{
		return sendBackNeeded;
	}

	HANDLE getDone()
	{
		return done;
	}

private:
	void AttemptAddToRP(UBMessage<T> message)
	{
		//check if the message's counter is in total_msgs
		unsigned int c = message.get_counter();
		bool inTotalMsgs = false;
		int alpha = -1;
		int numAfter = 0;

		//loops with iterators....
		std::list<UBMessage<T>>::iterator iter1 = delivered.begin();
		while (iter1 != delivered.end())
		{
			if (inTotalMsgs)
				++numAfter;

			if (iter1->get_counter() == c)
			{
				inTotalMsgs = true;
				numAfter = 0;
			}

			++iter1;
		}
		std::list<UBMessage<T>>::iterator iter2 = receive_pending.begin();
		while (iter2 != receive_pending.end())
		{
			if (inTotalMsgs)
				++numAfter;

			if (iter2->get_counter() == c)
			{
				inTotalMsgs = true;
				numAfter = 0;
			}

			++iter2;
		}
		//....end loops

		if (inTotalMsgs)
			alpha = numAfter;
		else
			alpha = gamma;

		if (alpha >= gamma)
		{
			AddToReceivePending(message);
		}
	}

	void AddToReceivePending(UBMessage<T> message)
	{
		unsigned int c = message.get_counter();

		if (receive_pending.size() == 0)
		{
			receive_pending.push_back(message);
			return;
		}

		unsigned int current = receive_pending.back().get_counter();
		if (MLT(current, c))
		{
			receive_pending.push_back(message);
			return;
		}

		unsigned int next = 0;
		std::list<UBMessage<T>>::iterator iter = receive_pending.end();
		iter--;
		for (int i = receive_pending.size() - 1; i >= 1; i--)
		{
			current = iter->get_counter();
			iter--;
			next = iter->get_counter();
			iter++;
			if (MLT(next, c) && MLT(c, current))
			{
				receive_pending.insert(iter, message);
				return;
			}

			iter--;
		}

		next = receive_pending.front().get_counter();
		if (MLT(c, next))
		{
			receive_pending.push_front(message);
			return;
		}
	}

	
	bool Analyze(int rcvdTag)
	{
		if(expectedEnabled.find(rcvdTag) != expectedEnabled.end())
		{
			//we're good - this tag does not contradict our gamma

			//check whether we have received this tag is trivially enabled (we have received it already)
			//if it has already been received, no modification is necessary
			if(!expectedEnabled[rcvdTag])
			{
				//now need to update expectedEnabled

				//1. MARK THE RECEIVED TAG AS RECEIVED
				expectedEnabled[rcvdTag] = true;

				//2. REMOVE TAGS THAT NEED TO BE REMOVED
				for (int i = 0; i < gamma; i++)
				{
					int maybeRemove = (rcvdTag - int(gamma) - i) % int(numTags);
					if (maybeRemove < 0) maybeRemove += numTags;

					//if the tag is not in the E-Set
					if (expectedEnabled.find(maybeRemove) == expectedEnabled.end() || MLT(rcvdTag, maybeRemove))
					{
						break; // don't remove any more
					}
					else
					{
						expectedEnabled.erase(maybeRemove);
					}
				}

				//3. ADD TAGS THAT NEED TO BE ADDED

				//if any older tags have not been received, no tags will be added
				//initialize the bool value that evaluates whether the potential addition will be actually added to the set
				bool pastFalses = false;
				//start with rcvdTag, go backwards and check gamma tags or until a tag is not in the set
				for (int i = 0; i < gamma; i++)
				{
					int checkFalseIndex = (rcvdTag - i) % int(numTags);
					if (checkFalseIndex < 0) checkFalseIndex += numTags;

					if (expectedEnabled.find(checkFalseIndex) != expectedEnabled.end())
					{
						if (expectedEnabled[checkFalseIndex] == false)
						{
							pastFalses = true;
							break;
						}
					}
					else
					{
						break;
					}
				}

				//add here
				if(!pastFalses)
				{
					int numInsertions = 0;

					for (int i = 0; i < gamma; i++)
					{
						//check tags ahead to see if the rcvdTag was a bottleneck
						//this shouldn't create empty elements because of the lazy property of the && operator
						if (expectedEnabled.find((rcvdTag + i) % numTags) != expectedEnabled.end() && expectedEnabled[(rcvdTag + i) % numTags] == true)
						{
							++numInsertions; //this should happen at least once, i.e. when i is 0
						}
						else
						{
							break;
						}
					}

					//make the additions
					for (int i = 0; i < numInsertions; i++)
					{
						expectedEnabled[(rcvdTag + int(gamma) + i) % int(numTags)] = false;
					}
				}
			}

			return true;
		}
		else
		{
			//bad! this tag contradicts our gamma
			//send a "bad" message to back_to_channel
			
			//when true, send_back is enabled
			sendBackNeeded = true;

			//1. Reset and update some variables and data structures for the next generation of tags (i.e. tags with an increased numTags value)
			counterB = 0;
			gamma += 1; //subject to adding more than 1; TODO: have a #define for the increase value
			numTags = gamma + delta_max;

			expectedEnabled.erase(expectedEnabled.begin(), expectedEnabled.end());
			//enable the first gamma tags
			for (unsigned int i = 0; i < gamma; i++)
			{
				expectedEnabled[i] = false;
			}

			//remove all from delivered
			while (!delivered.empty())
			{
				delivered.pop_front();
			}

			//2. Move messages in receive_pending into lower_tagged
			while (!receive_pending.empty())
			{
				UBMessage<T> tempMsg = receive_pending.front();
				receive_pending.pop_front();
				lower_tagged.push_back(tempMsg);
			}

			return false;
		}
	}
	

	bool MLT(unsigned int c1, unsigned int c2)
	{
		if (c1 < c2)
			return c2 - c1 <= gamma;

		return c1 - c2 > gamma;
	}
};




DWORD WINAPI receiverThread(LPVOID lpParam)
{
	printf("start receiver thread\n");

	Receiver<std::string> *receiverPtr = (Receiver<std::string>*) lpParam;

	//choose a seed based on the time
	srand(clock());

	//loop:
	while (true)
	{
		//check to see if we're done
		if (WaitForSingleObject(receiverPtr->getDone(), 1) == WAIT_OBJECT_0)
		{
			break;
		}

		//not done, so carry on
		//always receive when we can, because this event is begun in the channel
		if (receiverPtr->enabledreceive())
		{
			receiverPtr->receive();
		}

		bool send_back = receiverPtr->enabledsend_back();
		bool enabledRECEIVE = receiverPtr->enabledRECEIVE();
		int r = rand() % 2;

		switch (r)
		{
		//send_back(m)
		case 0:
			if (send_back)
			{
				receiverPtr->send_back();
			}
			break;
		//RECEIVE(m)
		case 1:
			if (enabledRECEIVE)
			{
				receiverPtr->RECEIVE();
			}
			break;
		//no default cases
		default:
			break;
		}
	}

	printf("end receiver thread\n");
	return 0;
}