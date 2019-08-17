#include "OutputEventData.h"

OutputEventData::OutputEventData(std::string message, ReplyType t)
{
	m = message;
	replyType = t;
}

ReplyType OutputEventData::getType() {
	return replyType;
}

std::string OutputEventData::getMessage() {
	return m;
}

OutputEventData::~OutputEventData()
{

}
