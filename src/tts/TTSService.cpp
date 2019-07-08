#include "TTSService.h"

TTSService::TTSService() : Service()
{
	currentlySpeaking.store(false);
	stopRequest.store(false);
}

void TTSService::addOnSpeechStart(void (*handler)(EventData *, std::atomic<bool> *)) {
	addListener(ON_SPEECH_START, handler);
}

void TTSService::addOnSpeechEnd(void (*handler)(EventData *, std::atomic<bool> *)) {
	addListener(ON_SPEECH_END, handler);
}

std::string TTSService::getStatusMessage() {
	return Service::generateNormalStatusMessage(getState(), getName());
}

TTSService::~TTSService() {

}
