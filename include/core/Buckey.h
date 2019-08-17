#ifndef BUCKEY_H
#define BUCKEY_H

#include "syslog.h"
#include "stdio.h"
#include "stdarg.h"
#include <mutex>
#include <queue>
#include <vector>
#include <string>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "yaml-cpp/yaml.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_mixer.h"

#include "SoundEffects.h"
#include "PromptResult.h"
#include "PromptEventData.h"

#include "Service.h"
#include "TTSService.h"

#include "Mode.h"
#include "CoreMode.h"
#include "EchoMode.h"

#include "EventSource.h"
#include "ModeControlEventData.h"
#include "ServiceControlEventData.h"
#include "ReplyType.h"

#include "cppfs/FileHandle.h"
#include "cppfs/FileIterator.h"
#include "cppfs/fs.h"

#include "DynamicGrammar.h"
#include "alternativeset.h"
#include "sequence.h"

#define ONINPUT "onInputEvent"
#define ONOUTPUT "onOutputEvent"

#define ONCONVERSATIONSTART "onConversationStartEvent"
#define ONCONVERSATIONEND "onConversationEndEvent"

#define ONENTERPROMPT "onEnterPromptEvent"
#define ONEXITPROMPT "onExitPrompEvent"

#define ONMODEREGISTER "onModeRegisterEvent"
#define ONMODEENABLE "onModeEnableEvent"
#define ONMODEDISABLE "onModeDisableEvent"
#define ONMODESTART "onModeStartEvent"
#define ONMODESTOP "onModeStopEvent"

#define ONSERVICEREGISTER "onServiceRegisterEvent"
#define ONSERVICEENABLE "onServiceEnableEvent"
#define ONSERVICEDISABLE "onServiceDisableEvent"
#define ONSERVICESTOP "onServiceStopEvent"
#define ONSERVICESTART "onServiceStartEvent"

#define ONFINISHINIT "onFinishInitEvent"

#define SOCKET_BUFFER_SIZE 64
#define MAX_COMMAND_SIZE 70

#define LOG_FILE "buckey.log"

typedef std::pair<bool, Service *> serviceListEntry;
typedef std::pair<bool, Mode *> modeListEntry;

typedef std::vector<serviceListEntry> servicesList;
typedef std::vector<modeListEntry> modesList;

///\brief The core class that is created and manages services, modes, input and output for the Buckey program
class Buckey : public EventSource {
	private:
    	void readCoreConfig();
    	void registerAllServices();
    	void registerAllModes();

    public:
		virtual ~Buckey();
		const char * name = "Buckey McAllister";

		///Set to true if the instance is starting up, running, or being destructed.
		const bool isRunning();

		///Set to true if child threads should stop running and the instance is going to be deleted
		const bool isKilled();

		///Requests for the instance to stop running, sets killed to true.
		void requestStop();


		void reload();

        //Services
        const ServiceState getServiceState(const std::string & serviceName);
        void startService(const std::string & serviceName);
        void stopService(const std::string & serviceName);
        void enableService(const std::string & serviceName);
        void disableService(const std::string & serviceName);
        void reloadService(const std::string & serviceName);
        void registerService(Service * service);
        const void listEnabledServices(std::vector<std::string> & output);
        const void listDisabledServices(std::vector<std::string> & output);
        const void listServices(std::vector<std::string> & output);
        const void listServicesInState(std::vector<std::string> & output, ServiceState s);
        const Service * getService(const std::string & serviceName);

        //Utility
        cppfs::FileHandle getTempFile(const std::string & extension);
        static void logDebug(std::string message);
        static void logInfo(std::string message);
        static void logWarn(std::string message);
        static void logError(std::string message);

        //Input
        void passInput(std::string input);
        void passCommand(std::string command);

        //Output & Conversation
        void startConversation();
        void endConversation();
        bool isInConversation();
        void reply(std::string message, ReplyType type);
        PromptResult * promptConfirmation(const std::string & prompt, int timeout = 7);

        //Sound
		bool isMakingSound();
		unsigned short countSoundsPlaying();
		bool playSoundEffect(SoundEffects effect, bool sync = false);

        //Modes
        DynamicGrammar * getRootGrammar();
        void addModeToRootGrammar(const Mode * m, DynamicGrammar * g);
        void removeModeFromRootGrammar(const Mode * m, DynamicGrammar * g);
        const void listEnabledModes(std::vector<std::string> & output);
        const void listDisabledModes(std::vector<std::string> & output);
        const void listModes(std::vector<std::string> & output);
        const void listModesInState(std::vector<std::string> & output, const ModeState s);
        void registerMode(Mode * m);
        void enableMode(const std::string & name);
        void disableMode(const std::string & name);
        void reloadMode(const std::string & name);
        void stopMode(const std::string & name);
        void startMode(const std::string & name);
        const ModeState getModeState(const std::string & name);

		//Singleton implementation
        static Buckey * getInstance() {
        	if(!instanceSet) {
				instanceSet = true;
				instance = new Buckey();
				instance->init();
        	}
        	return instance;
        };

        static Buckey * getInstance(cppfs::FileHandle configDir, cppfs::FileHandle assetsDir, cppfs::FileHandle tempDir) {
        	if(!instanceSet) {
				instanceSet = true;
				instance = new Buckey(configDir, assetsDir, tempDir);
				instance->init();
        	}
        	return instance;
        };

    protected:
    	Buckey();
        Buckey(cppfs::FileHandle configDir, cppfs::FileHandle assetsDir, cppfs::FileHandle tempDir);
    	void init();

    	//General
    	std::atomic<bool> running;
    	std::atomic<bool> killed;
    	///Bool is true if Service is enabled on startup
    	std::vector<std::pair<bool, Service *>> services;
    	///Bool is true if Mode is enabled on startup
    	std::vector<std::pair<bool, Mode *>> modes;

    	//Conversation
    	///Locked when a conversation is occurring. This way only 1 conversation happens at a time.
    	std::mutex conversationMutex;
    	std::atomic<bool> inConversation;

    	//Input Que
    	std::queue<std::string> inputQue;
    	/// Locked when adding or reading from the inputQue vector
    	std::mutex inputQueMutex;
    	std::thread inputWatcher;
    	static void watchInputQue(Buckey * b);

    	//Sounds
    	void initAudio();
    	std::atomic<unsigned short> soundsPlayingCount;
        std::vector<std::pair<SoundEffects, Mix_Chunk *>> soundBank;
        std::vector<int> soundEffectChannels;
        static void soundEffectDoneCallback(int channel);

    	//UNIX Socket Stuff
		std::thread socketManagementThread;
		int socketHandle, socket2Handle;
		struct sockaddr_un local, remote;
		void makeServerSocket();
		static void manageUnixSocket();
		std::atomic<bool> socketConnected;

    	//Config handles
    	cppfs::FileHandle coreConfigDir;
    	cppfs::FileHandle coreAssetsDir;
    	cppfs::FileHandle configDir;
    	cppfs::FileHandle assetsDir;
    	cppfs::FileHandle tempDir;
    	cppfs::FileHandle coreConfig;
    	YAML::Node coreConfigYAML;

    	//JSGF Grammar Stuff
    	DynamicGrammar * rootGrammar;
    	TTSService * ttsService;
    	CoreMode * coreMode;
    	shared_ptr<AlternativeSet> modeList;
    	shared_ptr<Sequence> rootExpansion;
    	std::mutex rootGrammarLock;

    	static bool instanceSet;
    	static Buckey * instance;
		static unsigned long nextTempID;
		static std::mutex tempIDLock;
		static FILE * logFile;
		static std::mutex logLock;
};

#endif // BUCKEY_H

/*! \mainpage Buckey
	\p Buckey is a privacy oriented, open-source, hackable personal assistant.
	\p The main documentation starts at Buckey.h
	\section Coding Conventions
	\p It is recommended you follow Buckey's coding conventions for consistency, readability, and re-usability.

	\li Files, Classes and method names are named with PascalCasing (the first letter to every word is a capital).
	\li Local variables are named in camelCase (the first letter is not capitalized, but the following first letters of later words are capitalized).
	\li Do not use snake_case.
	\li Please use the .cpp extension for C++ source files and the .c extension for C source files.
	\li Please use the .hpp extension for C++ header files and the .h extension for C header files.
	\li Please make use of the cppfs library. Yes, it provides many basic functions that could be rewritten in pure C++ and save executable size and memory, however the library provides cross-platform features that would take just as much size and memory to implement.
	\li Please read \ref event-handling to learn about event handling.
	\li You will learn that event handlers have to be statically accessible, so it is advisable that you provide a Singleton implementation when creating your own Modes and Services as their behavior will often be controlled by static methods.

	\p To learn more about how Buckey manages threads and memory, see \ref lifecycle

*/

/*!
	\page buckey-event-handlers List of Buckey's Events
	\p The Buckey class orchestrates the operation of the system, mainly through events.
	Services and Modes can attach listeners to these events to drive their own unique behavior.
	This page provides a list of string values for each of Buckey event types:

	\li ONINPUT "onInputEvent" - Called when the user passes input to Buckey.
	\li ONOUTPUT "onOutputEvent" - Called when Buckey passes output to the user.

	\li ONCONVERSATIONSTART "onConversationStart" - Called when a conversation starts.
	\li ONCONVERSATIONEND "onConversationEnd - Called when a conversation ends.
	\li ONENTERPROMPT "onEnterPrompt" - Called when a conversation enters a prompt.
	\li ONEXITPROMPT "onExitPrompt" - Called when a prompt is exited out (may be timed out!).

	\li ONMODEREGISTER "onModeRegisterEvent" - Called when a Mode is registered.
	\li ONMODEENABLE "onModeEnableEvent" - Called when a Mode is enabled.
	\li ONMODEDISABLE "onModeDisableEvent" - Called when a Mode is disabled.

	\li ONSERVICEREGISTER "onServiceRegisterEvent" - Called when a Service is registered.
	\li ONSERVICEENABLE "onServiceEnableEvent" - Called when a Service is enabled.
	\li ONSERVICEDISABLE "onServiceDisableEvent" - Called when a Service is disabled.

	\li ONFINISHINIT "onFinishInitEvent" - Called once Buckey registers and enables all needed Services and Modes.

*/
