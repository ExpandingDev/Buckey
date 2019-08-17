#ifndef SPHINXRECOGNIZER_H
#define SPHINXRECOGNIZER_H
#include <thread>
#include <vector>
#include <mutex>
#include <iterator>

//TODO: Portability for windows, replacing usleep and unistd.h
#include "unistd.h"

#include "yaml-cpp/yaml.h"

#include "ostream"
#include "stdio.h"
#include "cppfs/FileHandle.h"
#include "SphinxDecoder.h"
#include "SphinxHelper.h"
#include "EventSource.h"
#include "EventData.h"
#include "HypothesisEventData.h"
#include "grammar.h"
#include "DynamicGrammar.h"
#include "Service.h"
#include "PromptEventData.h"

#define AUDIO_FRAME_SIZE 2048

// String constants for events implemented by the recognizer
#define ON_START_SPEECH "onSpeechStart"
#define ON_END_SPEECH "onSpeechEnd"
#define ON_HYPOTHESIS "onHypothesis"
#define ON_PAUSE "onPause"
#define ON_RESUME "onResume"
#define ON_READY "onReady"
#define ON_SERVICE_READY "onServiceReady"

class SphinxService : public Service, public EventSource
{
	friend class Service;
    protected:
    	unsigned short maxDecoders;
    	std::atomic<bool> manageThreadRunning;
        std::atomic<unsigned short> currentDecoderIndex;
        std::vector<SphinxDecoder *> decoders;
        std::vector<std::thread> miscThreads;
        std::thread recognizerLoop;
        std::mutex decoderIndexLock;
        std::mutex updateLock;

        std::atomic<bool> inUtterance;
        std::atomic<bool> endLoop;
        std::atomic<bool> paused;
        std::atomic<bool> recognizing;
        std::atomic<bool> voiceDetected;
        std::atomic<bool> isRecording;
        std::atomic<bool> pressToSpeakMode;
        std::atomic<bool> pressToSpeakPressed;
        std::string recordingFile;

        static unsigned long onEnterPromptEventHandlerID;
		static unsigned long onConversationEndEventHandlerID;

        std::string deviceName;
        SphinxHelper::RecognitionSource source;

        static void manageContinuousDecoders(SphinxService *);
        static void manageNonContinuousDecoders(SphinxService * sr);
        static void endAndGetHypothesis(SphinxService *, SphinxDecoder *);

        FILE * recordingFileHandle;
        FILE * sourceFile;

        YAML::Node config;

        SphinxService();
        static SphinxService * instance;
		static std::atomic<bool> instanceSet;

    public:
		static SphinxService * getInstance();

        SphinxDecoder * getDecoder(unsigned short decoderIndex = 0);

        // Recognition
        void startContinuousDeviceRecognition(std::string device = "");
        void startPressToSpeakRecognition(std::string device = "");
        void startFileRecognition(std::string pathToFile);
        void stopRecognition();
        bool isRecognizing();
        bool voiceFound();

        //Push to speak
        ///Call when the press to speak button changes state to down.
        void pressToSpeakButtonDown();
        ///Call when the press to speak button changes state to up/released.
        void pressToSpeakButtonUp();
        ///Returns true if in press to speak mode
        bool inPressToSpeak();
        ///Returns true if the press to speak button is down
        bool pressToSpeakIsPressed();

        // Recording
        bool recordAudioToFile(std::string pathToRawFile);
        bool isRecordingToFile();
        void stopRecordingToFile();
        bool startRecordingToFile();

        // Interruption
        void pauseRecognition();
        void resumeRecognition();

        ///Returns true when continuous recognition is paused
        bool isPaused();

        //Dictionary
        bool wordExists(std::string word);
        void addWord(std::string word, std::string phones);

        //Updating decoders
        void updateDictionary(std::string pathToDictionary);
        void updateAcousticModel(std::string pathToAcousticMode);
        void updateJSGFPath(std::string pathToJSGF);
		void updateLMPath(std::string pathToLM);
		void updateSearchMode(SphinxHelper::SearchMode mode);
		void updateLogPath(std::string pathToLog);
		void setJSGF(Grammar * g);
        void applyUpdates();

        //Event handling
        void addOnSpeechStart(void (*handler)(EventData *, std::atomic<bool> *));
        void addOnSpeechEnd(void (*handler)(EventData *, std::atomic<bool> *));
        void addOnHypothesis(void (*handler)(EventData *, std::atomic<bool> *));
        void addOnReady(void (*handler)(EventData *, std::atomic<bool> *));
        void addOnNotReady(void (*handler)(EventData *, std::atomic<bool> *));
        void addOnResume(void (*handler)(EventData *, std::atomic<bool> *));
        void addOnPause(void (*handler)(EventData *, std::atomic<bool> *));

        void clearSpeechStartListeners();
        void clearSpeechEndListeners();
        void clearOnHypothesisListeners();
        void clearOnPauseListeners();
        void clearOnResumeListeners();

        static void onEnterPromptEventHandler(EventData * data, std::atomic<bool> * done);
		static void onConversationEndEventHandler(EventData * data, std::atomic<bool> * done);

        //Service
        void start();
        void stop();
        std::string getName() const;
        void setupAssets(cppfs::FileHandle aDir);
        void setupConfig(cppfs::FileHandle cDir);

        virtual ~SphinxService();
};

#endif // SPHINXRECOGNIZER_H
