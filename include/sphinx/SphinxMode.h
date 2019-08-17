#ifndef SPHINXMODE_H
#define SPHINXMODE_H

#include <Mode.h>
#include <ServiceControlEventData.h>
#include <DynamicGrammar.h>
#include <Buckey.h>

#include <cppfs/FileHandle.h>

#include <atomic>
#include <vector>
#include <string>

class SphinxMode : public Mode
{
	public:
		static SphinxMode * getInstance();

		void setupConfigDir(cppfs::FileHandle cDir);

		void setupAssetsDir(cppfs::FileHandle aDir);

		void input(std::string & command, std::vector<std::string> & tags);

		void start();
		void stop();

		static void serviceEventHandler(EventData * data, std::atomic<bool> * done);
		static void serviceDisabledEventHandler(EventData * data, std::atomic<bool> * done);
		static void serviceEnabledEventHandler(EventData * data, std::atomic<bool> * done);

		virtual ~SphinxMode();

	protected:
		SphinxMode();
		static SphinxMode * instance;
		static std::atomic<bool> instanceSet;
		DynamicGrammar * grammar;

		static std::atomic<bool> sphinxRunning;

		static unsigned long serviceEnableHandler;
		static unsigned long serviceDisableHandler;
		static unsigned long serviceRegisterHandler;
};

#endif // SPHINXMODE_H
