#ifndef TTSSERVICE_H
#define TTSSERVICE_H
#include <atomic>
#include <string>

#include "Service.h"
#include <EventSource.h>

#define ON_SPEECH_START "onTTS_Start"
#define ON_SPEECH_END "onTTS_End"

///\brief Abstract class that should be extended and implemented by Services to provide TTS capability.
class TTSService : public Service, public EventSource {

public:
	TTSService();
	virtual ~TTSService();

	// Service State
	virtual std::string getStatusMessage();

	//Speaking
	virtual int speak(std::string words) = 0;
	virtual void asyncSpeak(std::string words) = 0;
	virtual bool stopSpeaking() = 0;

	//History
	virtual unsigned long getHistorySize() = 0;
	virtual std::string lastSaid() = 0;
	virtual std::string wordsSaidAt(unsigned long index = 0) = 0;

	//State
	virtual bool isSpeaking() = 0;

	//Event handling
	void addOnSpeechStart(void (*handler)(EventData *, std::atomic<bool> *));
	void addOnSpeechEnd(void (*handler)(EventData *, std::atomic<bool> *));

protected:
	std::atomic<ServiceState> state;
	std::atomic<bool> enabled;
	std::atomic<bool> running;
	std::atomic<bool> currentlySpeaking;
	std::atomic<bool> stopRequest;

};

#endif
