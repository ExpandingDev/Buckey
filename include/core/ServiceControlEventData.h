#ifndef SERVICECONTROLEVENTDATA_H
#define SERVICECONTROLEVENTDATA_H
#include <EventData.h>
#include <Service.h>

///\brief An extension of the EventData class that is used whenever a Service is registered, enabled, disabled, or stopped.
///
///		An extension of the EventData class that is used whenever a Service is registered, enabled, disabled, or stopped.
///		Holds a pointer to the Service in question
class ServiceControlEventData : public EventData
{
	public:
		ServiceControlEventData(Service * s);
		virtual ~ServiceControlEventData();

		///Returns the stored Service pointer
		Service * getService() const { return service; }

	protected:
		///Stored pointer to the Service
		Service * service;
};

#endif // SERVICECONTROLEVENTDATA_H
