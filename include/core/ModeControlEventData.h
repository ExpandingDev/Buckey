#ifndef MODECONTROLEVENTDATA_H
#define MODECONTROLEVENTDATA_H

#include <EventData.h>
#include <Mode.h>

///\brief EventData class to hold information when a Mode is registered, enabled, or disabled.
///
///Users should really never have to touch this class, used by Buckey and the CoreMode
class ModeControlEventData : public EventData
{
	public:
		///\brief Construct a new ModeControlEventData object and mark the given mode at the Mode that triggered this event.
		///\param mode [in] Pointer to the Mode that is considered to have triggered this event.
		ModeControlEventData(Mode * m);
		virtual ~ModeControlEventData();
		///\brief Returns a pointer to the Mode that triggered the event
		///\return Pointer to the Mode that triggered the event
		Mode * getMode();

	protected:
		///\brief Pointer to the Mode that triggered the event
		Mode * mode;
};

#endif // MODECONTROLEVENTDATA_H
