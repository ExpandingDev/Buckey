#ifndef ASYNCSPEECHREQUESTEVENTDATA_H
#define ASYNCSPEECHREQUESTEVENTDATA_H
#include <string>

#include <EventData.h>
#include <TTSService.h>

///EventData class used exclusively by MimicTTS Service that is passed to the async event listener to request words be spoken asyncronously
class AsyncSpeechRequestEventData : public EventData
{
	public:
		AsyncSpeechRequestEventData(std::string words, TTSService * service);
		virtual ~AsyncSpeechRequestEventData();
		///Pointer to the TTS Service because event handlers are static
		TTSService * tts;
		///Words that are requested to be spoken
		std::string text;
};

#endif // ASYNCSPEECHREQUESTEVENTDATA_H
