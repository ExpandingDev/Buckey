#ifndef OUTPUTEVENTDATA_H
#define OUTPUTEVENTDATA_H
#include <string>

#include <EventData.h>
#include <ReplyType.h>

///\brief An EventData class specialized to hold data about reply() requests to Buckey.
///
///Holds data from a reply() request after the reply has been outputted to the console and/or the UNIX socket.
class OutputEventData : public EventData
{
	public:
		///\brief Construct a new OutputEventData object and store the given message and ReplyType
		///\param message [in] The message to store
		///\param type [in] The ReplyType to store
		OutputEventData(std::string message, ReplyType t);
		virtual ~OutputEventData();

		///\brief Returns the message
		///\return std::string The stored message that was outputted by Buckey
		std::string getMessage();
		///\brief Returns the ReplyType
		///\return ReplyType The stored ReplyType
		ReplyType getType();

	protected:
		///\brief The stored ReplyType
		ReplyType replyType;
		///\brief The stored message
		std::string m;
};

#endif // OUTPUTEVENTDATA_H
