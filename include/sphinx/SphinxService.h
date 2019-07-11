#ifndef SPHINXRECOGNIZER_H
#define SPHINXRECOGNIZER_H
#include <thread>
#include <vector>
#include <mutex>
#include <iterator>
#include "ostream"
#include "stdio.h"
#include "syslog.h"

//TODO: Portability for windows, replacing usleep and unistd.h
#include "unistd.h"

#include "yaml-cpp/yaml.h"

#include "cppfs/FileHandle.h"

#include "grammar.h"

#include "Service.h"
#include "SphinxDecoder.h"
#include "SphinxHelper.h"
#include "EventData.h"
#include "EventSource.h"

#include "pocketsphinx.h"
#include "pocketsphinx_export.h"
#include "sphinxbase/cmd_ln.h"
#include "dict.h"

#define AUDIO_FRAME_SIZE 2048

// String constants for events implemented by the recognizer
#define ON_START_SPEECH "onSpeechStart"
#define ON_END_SPEECH "onSpeechEnd"
#define ON_HYPOTHESIS "onHypothesis"
#define ON_READY "onReady"
#define ON_PAUSE "onPause"
#define ON_RESUME "onResume"
#define ON_SERVICE_READY "onServiceReady"

///\brief A Service that provides speech recognition using the CMU Pocketsphinx library.
///
///A Service that provides speech recognition using the CMU Pocketsphinx library.
///Can create multiple SphinxDecoders to run simultaneously, so that as one decoder is unavailable because of processing output or being reconfigured, other decoders are available to process user input.
///Speech recognition can be paused and resumed, and can also record audio to a file while speech recognition is occurring.
///Contains its own thread that run to manage the multiple SphinxDecoders and orchestrates updating decoder configuration while maximizing decoder uptime.
class SphinxService : public Service, public EventSource
{
	friend class Service;
    protected:

    	///Creates the default search modes on the sphinx decoder
		void createDefaultSearches(ps_decoder_t * p);
		///Creates the default number of decoders.
		void createDefaultDecoders();
		std::vector<SphinxDecoder *> decoders;

        SphinxService();

        std::string defaultLogPath;
        std::string defaultDictionaryPath;
        std::string defaultHMMPath;
        std::string defaultLMPath;

        YAML::Node config;
        static SphinxService * instance;
		static std::atomic<bool> instanceSet;

    public:
		static SphinxService * getInstance();

		SphinxDecoder * createDecoder(std::string name);

		//Search modes
		void addLMSearchMode(std::string pathToLM, std::string name);
		void addJSGFStringSearchMode(std::string jsgfString, std::string name);
		void addJSGFFileSearchMode(std::string pathToJSGF, std::string name);
		void addKeywordSearchMode(std::string keyword, std::string name);

		bool wordExists(std::string word);

		unsigned short getNumberOfDecoders();

        //Service
        void enable();
        void disable();
        void stop();
        std::string getName() const;
        void setupAssets(cppfs::FileHandle aDir);
        void setupConfig(cppfs::FileHandle cDir);

        virtual ~SphinxService();
};

#endif // SPHINXRECOGNIZER_H
