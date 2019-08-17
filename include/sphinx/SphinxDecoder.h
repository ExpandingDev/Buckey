#ifndef SPHINXDECODER_H
#define SPHINXDECODER_H
#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>
#include "SphinxHelper.h"
#include "pocketsphinx.h"
#include "cmd_ln.h"
#include <string>
#include <iostream>
#include <atomic>

#define DEFAULT_HMM_PATH "/usr/local/share/pocketsphinx/model/en-us/en-us"
#define DEFAULT_DICT_PATH "/usr/local/share/pocketsphinx/model/en-us/cmudict-en-us.dict"
#define DEFAULT_LM_PATH "/usr/local/share/pocketsphinx/model/en-us/en-us.lm.bin"
#define DEFAULT_LOG_PATH "/dev/null"

/// All functions (and constructors and destructors) are synchronous. Any asynchronous tasks should be carried out by a managing class (SphinxRecognizer).
/// This class serves as a bare bones C++ wrapper for the CMU pocketsphinx library with a few added convenience functions.
class SphinxDecoder
{
    friend class SphinxService;
    public:
        /// The pathToSearchFile is either the path to the language model or the path to the JSGF grammar. Depends on the specified searchMode.
        SphinxDecoder(std::string decoderName, std::string pathToSearchFile = DEFAULT_LM_PATH, SphinxHelper::SearchMode searchMode = SphinxHelper::SearchMode::LM, std::string pathToHMM = DEFAULT_HMM_PATH, std::string pathToDictionary = DEFAULT_DICT_PATH, std::string pathToLogFile = DEFAULT_LOG_PATH, bool doInit = true);
        ~SphinxDecoder();

        const bool isReady();
        bool processRawAudio(int16 adbuf[], int32 frameCount);

        // Dictionary manipulation
        const bool wordExists(std::string word);
        void addWord(std::string word, std::string phonemes);

        //Utterance
        void startUtterance();
        void endUtterance();
        std::string getHypothesis();

        //Updating methods
        void updateAcousticModel(std::string pathToHMM, bool forceUpdate = true);
        void updateDictionary(std::string pathToDict, bool forceUpdate = true);
		void updateLM(std::string pathToLM, bool forceUpdate = true);
        void updateJSGF(std::string pathToJSGF, bool forceUpdate = true);
		void updateSearchMode(SphinxHelper::SearchMode mode, bool forceUpdate = true);
		void updateLoggingFile(std::string pathToLog, bool forceUpdate = true);

        //Getters
        std::string getDictionaryPath();
        std::string getHMMPath();
        std::string getJSGFPath();
        std::string getLMPath();
        std::string getLogPath();
        std::string getName();
        const SphinxHelper::DecoderState getState();

        cmd_ln_t * getConfig();

    protected:
        std::string dictionaryPath; // path to the dictionary file
        std::string hmmPath; // path to the acoustic model
        std::string jsgfPath; // path to the jsgf grammar
        std::string lmPath; // path to the language model
		std::string logPath; // path to the logging file
		std::string name;

		SphinxHelper::SearchMode recognitionMode;
		bool ready;
		bool inUtterance;

		std::atomic<SphinxHelper::DecoderState> state;

        void reloadDecoder();

        ps_decoder_t *ps;
        cmd_ln_t *config;
};

#endif // SPHINXDECODER_H
