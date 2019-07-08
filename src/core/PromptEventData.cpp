#include "PromptEventData.h"

PromptEventData::PromptEventData(std::string type)
{
	t = type;
}

PromptEventData::~PromptEventData()
{
	//dtor
}

std::string PromptEventData::getType() {
	return t;
}
