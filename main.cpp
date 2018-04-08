#include <windows.h>
#include <ctime>
#include "sender.h"
#include "channel.h"
#include "receiver.h"

//DELTA must be within the range [DELTA_MIN, DELTA_MAX]
//The initial number of tags will be DELTA_MIN + DELTA_MAX
#define DELTA_MIN 3
#define DELTA_MAX 20
#define DELTA 20
#define MSGNUM 500

int main()
{
	if (DELTA < DELTA_MIN || DELTA > DELTA_MAX)
	{
		printf("error: invalid #define(s)\n");
		return 1;
	}

	printf("start main thread\n");

	//create the shared queues
	Queue<std::string> to_sender;
	Queue<UBMessage<std::string>> to_channel;
	Queue<UBMessage<std::string>> to_receiver;
	Queue<UBMessage<std::string>> back_to_channel;
	Queue<UBMessage<std::string>> back_to_sender;
	Queue<UBMessage<std::string>> to_final;

	//create the event handle
	HANDLE done = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (done == NULL)
	{
		printf("error creating the done event\n");
		return 1;
	}

	//create the automata
	Sender<std::string> senderA(DELTA_MIN, DELTA_MAX, &to_sender, &to_channel, &back_to_sender, done); //add back_to_sender
	Channel<std::string> channelC(DELTA, &to_channel, &to_receiver, &back_to_channel, &back_to_sender, done); //add back_to_sender, back_to_channel
	Receiver<std::string> receiverB(DELTA_MIN, DELTA_MAX, &to_receiver, &to_final, &back_to_channel, done); //add back_to_channel

	//load the to_sender queue
	for (int i = 1; i <= MSGNUM; ++i)
	{
			std::string msg = "This is message #" + std::to_string(i);
			to_sender.enqueue(msg);
	}

	//launch threads
	if (CreateThread(NULL, 0, senderThread, (LPVOID)(&senderA), NULL, NULL) == NULL)
	{
		printf("error creating the sender thread\n");
		return 2;
	}
	if (CreateThread(NULL, 0, channelThread, (LPVOID)(&channelC), NULL, NULL) == NULL)
	{
		printf("error creating the channel thread\n");
		return 3;
	}
	if (CreateThread(NULL, 0, receiverThread, (LPVOID)(&receiverB), NULL, NULL) == NULL)
	{
		printf("error creating the receiver thread\n");
		return 4;
	}

	//loop:
	while (true)
	{
		//TODO: Note that as the program currently works,
		//		unless delta messages in the last generation of numTags values are sent to the channel,
		//		messages in older generations will remain enabled in_transit and will take priority to be RECEIVED(),
		//		so the program will output those older messages infinitely.

		//check to see if we're done
		//this will no longer work... how about we:
		//A. make sure that the size is at least MSGNUM
		//B. make sure that the most recent addition to to_final matches the final high-level message
		//
		
		/*if (to_sender.isEmpty() && to_final.size() == MSGNUM)
		{
			SetEvent(done);
			break;
		}*/

		//alternatively, we could have no exit condition and just print any messages in to_final; Ctrl-C to stop the program
		if (to_final.size() > 0)
		{
			UBMessage<std::string> m = to_final.dequeue();
			printf("%s (numTags = %u)\n", m.get_contents().c_str(), m.get_numTags());
		}

		//not done, so carry on
	}

	//print all the received messages in order
	for (int i = 0; i < MSGNUM; ++i)
	{
		UBMessage<std::string> m = to_final.dequeue();
		printf("%s (numTags = %u)\n", m.get_contents().c_str(), m.get_numTags());
	}

	printf("end main thread\n");
	return 0;
}