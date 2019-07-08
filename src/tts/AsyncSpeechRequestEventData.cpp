#include "AsyncSpeechRequestEventData.h"

AsyncSpeechRequestEventData::AsyncSpeechRequestEventData(std::string words, TTSService * s)
{
	tts = s;
	text = words;
}

AsyncSpeechRequestEventData::~AsyncSpeechRequestEventData()
{
	//dtor
}
