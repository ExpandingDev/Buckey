#ifndef EVENTSOURCE_H
#define EVENTSOURCE_H
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <unordered_map>

#include <EventData.h>

typedef std::vector<std::pair<std::string,std::vector<void(*)(EventData *, std::atomic<bool> *)>>>::iterator HandlerIterator;

///\brief A class meant to be extended that provides event triggering and event listening capabilities to a class
class EventSource
{
    public:
        EventSource();

        /// Adds an event listener of the specified type. The callback function should accept a pointer to an EventData object and a pointer to a atomic<bool> object. Before the callback exits it needs to set the atomic<bool> to true for thread cleanup.
        unsigned long addListener(std::string eventType, void (*)(EventData *, std::atomic<bool> *));

        /// Clears all event listeners for the given eventType
        void clearListeners(std::string eventType);

        /// Clear the event listener of the given handle
        void unsetListener(std::string eventType, unsigned long id);

        /// Returns the number of event listeners registered for the given eventType
        unsigned long listenerCount(std::string eventType);

        /// Returns the number of actively running threads that were triggered by events
        unsigned long handlerCount();

    protected:
        ///Triggers all events of the specified type and passes the specified args to them
        void triggerEvents(std::string eventType, EventData * arg);

        ~EventSource();

    	///Called by the destructor to join all event listener threads
        void killThreads();

    private:

        /// Locked when the event listener list is going to be invalidated or when the event listener or thread list is going to be used/invalidated.
        std::mutex threadManipulationLock;

        /// Setting this to true will lock additional events from firing and make the managerThread begin to close and end.
        std::atomic<bool> requestJoin;

        ///A vector of all of the event listener threads that were triggered and are running.
        std::vector<std::pair<std::thread *,std::atomic<bool> *> *> threads; // atomic<bool> is set to false while running, an set to true when done

		///The next available event listener handle ID
    	unsigned long nextID;

    	///Locked whenever reading or writing to the nextID variable
    	std::mutex idLock;

        ///Unordered map is organized by event types, vectors contain pairs consisting of handler ID and handler function pointer
        std::unordered_map<std::string, std::vector<std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>>> handlers;


        static void manageThreads(EventSource * es);

        /// Loop that cleans up exited event listeners.
        std::thread managerThread;
};

#endif // EVENTSOURCE_H

/*! \page event-handling How Event Handling Works
	\p Buckey makes use of the std::thread very often for asynchronous event handling. This is to allow for more fluid interaction with the user as multiple input, output, and computing processes can be running at once and not waiting for each to complete.
	For example, I could ask Buckey to test my Internet download speed, which would take a minute or two for a reliable measurement. Imagine having to wait for two minutes until you can enter another command once the test is done.
	Buckey provides a class for this, the EventSource class. If your object is going to trigger events, have it extend the EventSource class.
	The EventSource class takes care of its own threads and memory management, so you do not have to worry about specialized constructors or destructors.

	\section event-conventions Event Listener Conventions
	\p When creating Modes and Services, it is recommended that:
	\li Register and store event handler IDs when your enable() method is called (Services or Modes)
	\li Unset event handlers using the stored ID's when stop() is called on your Service, or when disable() is called on your Mode.
	\li Store your event handler IDs in protected static fields.
	\li If using the onModeEnable/Disable/Register, onServiceEnable/Disable/Register event listeners, it might be best to wait to register your event listeners once the Buckey::onInitFinishedEvent is triggered.
	\li Make your event handlers do short synchronous tasks. If their task will take a while to run, then spin up another std::thread, or have it periodically check that Buckey has not been killed.
	\li When naming your event types, it is recommended that you start with "on" and end with "Event", for example: "onMyCustomEvent"
	\li When naming your event handlers, it is recommended that you start with the event type and end with "Handler", for example: "onMyCustomEventHandler"
	\li When naming the event handler IDs, it is recommended that you name them the same as your event handler method, but with "ID" on the end, example: "onMyCustomEventHandlerID"

	\subsection triggering-event Triggering EventSource Events
	\p To trigger an event from your object, call the void triggerEvents(std::string type, EventData * data) method.
	The type string is the type of event that will be triggered. Listeners must be listening for this exact string. The EventData class is a generic class to hold data for events.
	Many times it is extended to hold data for a specific purpose, however if you do not want to pass any data, it is advisable to create a new EventData object and leave it blank.
	Do not worry about deleting your EventData object that you have allocated, it is internally deleted once all triggered event listeners have exited.

	\subsection registering-event-listeners Registering Event Listeners
	\p To register an event handler with an object that extends the EventSource, call the public unsigned long addListener(std::string eventType, void (*)(EventData *, std::atomic<bool> *)) method.
	The string eventType argument should be the same string that is used when triggering the event.
	The void (*)(EventData *, std::atomic<bool> *) argument that accepts a pointer to a method that requires EventData * and std::atomic<booL> * as arguments and returns void. This function must be statically accessible.
	The addListener method returns a ULONG (unsigned long) data type. This is the ID of your event listener. <b> Store this event listener ID. You will need it when you go to unset your event listener.</b>

	\subsection unsetting-event-listeners Removing/Unsetting Event Listeners
	\p To stop listening for an event, call the public method void unsetListener(std::string eventType, unsigned long id).
	NOTE: The event handler ID that you were passed will no longer be valid!

	\subsection further-reading Further Reading/Refernces
	\p To learn about the event types triggered by Buckey, look at \ref buckey-event-handlers
*/
