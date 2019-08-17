#include "EventData.h"

EventData::EventData()
{
    //ctor
}

EventData::EventData(int j) {
	i = j;
}

EventData::EventData(bool k) {
	b = k;
}

EventData::EventData(std::string t) {
	s = t;
}

int EventData::getInt() {
	return i;
}

bool EventData::getBool() {
	return b;
}

std::string EventData::getString() {
	return s;
}

EventData::~EventData()
{
    //dtor
}
