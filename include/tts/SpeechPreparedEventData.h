#ifndef SPEECHPREPAREDEVENTDATA_H
#define SPEECHPREPAREDEVENTDATA_H
#include <EventData.h>

///EventData class that holds information about the words and the resulting audio filename after speech has been prepared by a TTS Service
class SpeechPreparedEventData : public EventData
{
	public:
		SpeechPreparedEventData();
		SpeechPreparedEventData(std::string text, std::string fileName);
		virtual ~SpeechPreparedEventData();
		///The words that were prepared
		const std::string words;

		///The file path of the audio file where the prepared speech is stored
		const std::string filePath;

	protected:

	private:
};

#endif // SPEECHPREPAREDEVENTDATA_H
