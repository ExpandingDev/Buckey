#ifndef PROMPTEVENTDATA_H
#define PROMPTEVENTDATA_H

#include <EventData.h>

class PromptEventData : public EventData
{
	public:
		PromptEventData(std::string type);
		std::string getType();
		virtual ~PromptEventData();

	protected:
		///Internal string storing the type of prompt entered
		std::string t;

	private:
};

#endif // PROMPTEVENTDATA_H
