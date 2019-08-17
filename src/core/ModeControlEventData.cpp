#include "ModeControlEventData.h"

ModeControlEventData::ModeControlEventData(Mode * m)
{
	mode = m;
}

Mode * ModeControlEventData::getMode() {
	return mode;
}

ModeControlEventData::~ModeControlEventData()
{
	//dtor
}
