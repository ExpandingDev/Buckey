#ifndef COREMODE_H
#define COREMODE_H

#include <Mode.h>
#include <DynamicGrammar.h>
#include <vector>
#include <string>
#include "EventData.h"

///\brief The mode this is always enabled by Buckey that allows for mangement of modes and services.
class CoreMode : public Mode
{
	public:

		virtual ~CoreMode();

		void start();
		void stop();

		void setupAssetsDir(cppfs::FileHandle aDir);
		void setupConfigDir(cppfs::FileHandle cDir);

		void input(std::string & command, std::vector<std::string> & tags);

		static CoreMode * getInstance();

		///Event handler that is attached to Buckey that is called once all Modes and Services are registered and enabled
		static void initFinished(EventData * data, std::atomic<bool> * done);

		///Event handler that is attached to Buckey that is called whenever a Mode or Service changes states
		static void updateGrammar(EventData * data, std::atomic<bool> * done);

	protected:
		CoreMode();
		static DynamicGrammar * grammar;
		static cppfs::FileHandle templateGrammarHandle;
		static CoreMode * instance;
		static bool instanceSet;

		static unsigned long finishHandler;
		static unsigned long registerHandler;
		static unsigned long enableHandler;
		static unsigned long disableHandler;

		static unsigned long serviceDisableHandler;
		static unsigned long serviceEnableHandler;
		static unsigned long serviceRegisterHandler;
};

#endif // COREMODE_H
