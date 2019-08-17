#include "EventSource.h"

EventSource::EventSource() : requestJoin(false)
{
    threadManipulationLock.lock();
    managerThread = std::thread(EventSource::manageThreads, this);
    threadManipulationLock.unlock();
}

EventSource::~EventSource()
{
    killThreads();
}

unsigned long EventSource::handlerCount() {
	return threads.size();
}

void EventSource::killThreads() {
    requestJoin.store(true);
    threadManipulationLock.lock();
    managerThread.join();
    for(std::pair<std::thread *, std::atomic<bool> *> * p : threads) {
        p->first->join();
        delete p->first;
        delete p->second;
        delete p;
    }
    threads.clear();
    threadManipulationLock.unlock();
}

void EventSource::manageThreads(EventSource * es) {
    while(!es->requestJoin) {
        es->threadManipulationLock.lock();
        bool done = false;
        while(!done) {
			done = true;
			for(std::vector<std::pair<std::thread *, std::atomic<bool> *> *>::iterator i = es->threads.begin(); i != es->threads.end(); i++) {
				std::pair<std::thread *,std::atomic<bool> *> * p = *i;
				if(p->second->load()) {
					p->first->join();
					delete p->first;
					delete p->second;
					delete p;
					es->threads.erase(i);
					done = false;
					break;
				}
			}
        }
        es->threadManipulationLock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void EventSource::triggerEvents(std::string eventType, EventData * arg) {
	if(!requestJoin) { // Only trigger if we are still running.
		threadManipulationLock.lock();
		/*for(std::pair<std::string,std::vector<void(*)(EventData *, std::atomic<bool> *)>> p : handlers) {
			if(p.first == eventType) {
				for(void (*h)(EventData *, std::atomic<bool> *) : p.second) {
					std::atomic<bool> * doneAtomic = new std::atomic<bool>(false);
					std::thread * eventThread = new std::thread(h, arg, doneAtomic);
					std::pair<std::thread *,std::atomic<bool> *> * newPair = new std::pair<std::thread *,std::atomic<bool> *>(eventThread,doneAtomic);
					threads.push_back(newPair);
				}
			}
		}*/
		if(handlers.count(eventType) > 0) {
			std::vector<std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>> & methods = handlers[eventType];
			for(std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)> p : methods) {
				std::atomic<bool> * doneAtomic = new std::atomic<bool>(false);
				std::thread * eventThread = new std::thread(p.second, arg, doneAtomic);
				std::pair<std::thread *,std::atomic<bool> *> * newPair = new std::pair<std::thread *,std::atomic<bool> *>(eventThread,doneAtomic);
				threads.push_back(newPair);
			}
		}
		threadManipulationLock.unlock();
	}
}

// The below function does not invalidate the event list, so it does not require locking
unsigned long EventSource::addListener(std::string eventType, void(*handler)(EventData *, std::atomic<bool> *)) {
	idLock.lock();
	unsigned long id = nextID;
	nextID++;

	if(handlers.count(eventType) <= 0) {
		std::vector<std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>> newVector;
		newVector.push_back(std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>(id, handler));
		handlers[eventType] = newVector;
	}
	else {
		std::vector<std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>> & methods = handlers[eventType];
		methods.push_back(std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>(id, handler));
	}

    idLock.unlock();
   	return id;
}

//This function does invalidate the event list, so it does require locking!
void EventSource::clearListeners(std::string eventType) {
	threadManipulationLock.lock();
	if(handlers.count(eventType) > 0) {
		handlers.erase(eventType);
	}
	threadManipulationLock.unlock();
}

void EventSource::unsetListener(std::string eventType, unsigned long id) {
	threadManipulationLock.lock();
	if(handlers.count(eventType) > 0) {
		std::vector<std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>> & methods = handlers[eventType];
        for(std::vector<std::pair<unsigned long, void(*)(EventData *, std::atomic<bool> *)>>::iterator i = methods.begin(); i != methods.end(); i++) {
			if((*i).first == id) {
				methods.erase(i);
				break;
			}
        }
	}
	threadManipulationLock.unlock();
}
