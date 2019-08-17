#ifndef MIMICTTSSERVICE_H
#define MIMICTTSSERVICE_H
#include <fstream>
#include <ostream>
#include <memory>

#include "mimic.h"
#include "usenglish.h"
#include "cmu_lex.h"
#include "cst_wave.h"
#include "cst_audio.h"

#include <cppfs/fs.h>
#include <cppfs/FileHandle.h>
#include <cppfs/FilePath.h>

#include <TTSService.h>

#define ON_MIMIC_AUDIO_PREPARED "onMimicAudioPrepared"
#define ASYNC_SPEECH_REQUEST "onAsyncSpeechRequest"

///\brief An implementation of TTS Service using the Mimic-1 library.
///
///An implementation of TTS Service using the Mimic-1 library, includes methods to perform TTS earlier to allow for quicker reply times.
class MimicTTSService : public TTSService
{
	friend class Service;
	public:

		///\brief Returns the singleton instance of MimicTTSService
		static MimicTTSService * getInstance();

		virtual ~MimicTTSService();

		//Speech
		///\brief Synchronously speaks the given words
		///\param words [in] Words to speak
		int speak(std::string words);

		///\brief Asyncronously speaks the given words
		///\param words [in] Words to speak
		void asyncSpeak(std::string words);

		///\brief Performs TTS on the given words and stores the resulting WAV for later use
		///\param words [in] Words to prepare
		void prepareSpeech(std::string words);

		///\brief Performs TTS on the given vector of words and stores the resulting WAV files for later use
		///\param wordList [in] vectory of strings representing a list of words to prepare
		void prepareList(std::vector<std::string> & wordList);

		///\brief Plays the stored WAV file that was generated earlier by prepareSpeech or prepareList
		///\param words [in] The already prepared words to speak
		bool speakPreparedSpeech(std::string words);

		//State
		///\brief Returns true if the service is outputting sound from TTS
		bool isSpeaking();
		///\brief Requests the service to stop speaking, note, work in progress!
		bool stopSpeaking();

		//History
		///\brief Returns the number of items in the TTS history
		unsigned long getHistorySize();
		///\brief Returns the last words spoken
		///\return The last words spoken
		std::string lastSaid();
		///\brief Returns the words spoken at the given history index
		///\param index [in] The requested index to query. Default is 0.
		///\return The words spoken at that history index
		std::string wordsSaidAt(unsigned long index = 0);

		// Service
		void start();
		void stop();
		void setupConfig(cppfs::FileHandle cDir);
		void setupAssets(cppfs::FileHandle aDir);
		std::string getName() const;
		std::string getStatusMessage() const;

		//Events
		///Add an event listener for when the service finishes prepareSpeech or prepareList
		void addOnSpeechPrepared(void (*handler)(EventData *, std::atomic<bool> *));

		///Event handler that is registered with Buckey and is passed any reply calls and speaks what should be spoken
		static void handleOutputs(EventData * data, std::atomic<bool> * done);

	protected:
		///Create a new singleton instance if one does not exist already
		MimicTTSService();
		///Set to true if a singleton instance has been created
		static std::atomic<bool> instanceSet;
		///Pointer to the singleton instance
		static MimicTTSService * instance;

		///Internal method that is made into a std::thread when speakAsync is called
		static void doAsyncRequest(EventData * data, std::atomic<bool> * done);

		///List of all prepared words and their respective filenames
		std::vector<std::pair<std::string, std::string>> preparedAudio;
		///The select voice
		cst_voice * voice;
		///Returns the next available WAV file for prepared speech
		std::string getAvailableAudioFile();
		///Internal method that reads the prepared speech list on startup
		void loadPreparedAudioList();
		///Holds a record of all words spoken by the Service
		std::vector<std::string> history;

		cppfs::FileHandle preparedAudioList;
		unsigned int lastAudioIndex;
		YAML::Node config;

		///Event listener handle for the handleOutputs event listener
		unsigned long onOutputHandle;

	private:
		int error;
};

#endif // MIMICTTSSERVICE_H
