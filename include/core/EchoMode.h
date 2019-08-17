#ifndef ECHOMODE_H
#define ECHOMODE_H

#include <vector>
#include <string>
#include <atomic>

#include "cppfs/FileHandle.h"

#include <Mode.h>
#include "DynamicGrammar.h"

///A simple test mode to ensure Buckey is working
class EchoMode : public Mode
{
	public:
		static EchoMode * getInstance();
		void start();
		void stop();

		void setupAssetsDir(cppfs::FileHandle aDir);
		void setupConfigDir(cppfs::FileHandle cDir);

		void input(std::string & command, std::vector<std::string> & tags);
		virtual ~EchoMode();

	protected:
		DynamicGrammar * grammar;
		EchoMode();
		static std::atomic<bool> instanceSet;
		static EchoMode * instance;
};

#endif // ECHOMODE_H
