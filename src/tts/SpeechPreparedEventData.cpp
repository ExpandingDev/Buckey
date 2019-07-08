#include "SpeechPreparedEventData.h"

SpeechPreparedEventData::SpeechPreparedEventData() : words(""), filePath("")
{

}

SpeechPreparedEventData::SpeechPreparedEventData(std::string text, std::string fileName) : words(text), filePath(fileName)
{

}

SpeechPreparedEventData::~SpeechPreparedEventData()
{
	//dtor
}
