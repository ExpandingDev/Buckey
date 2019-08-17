#include "sphinx/SphinxDecoder.h"
#include <iostream>
#include "Buckey.h"

SphinxDecoder::SphinxDecoder(std::string decoderName, std::string pathToSearchFile, SphinxHelper::SearchMode searchMode, std::string pathToHMM, std::string pathToDictionary, std::string pathToLogFile, bool doInit) {
    name = decoderName;
    state.store(SphinxHelper::DecoderState::NOT_INITIALIZED);
    ready = false;
    hmmPath = pathToHMM;
    dictionaryPath = pathToDictionary;
	recognitionMode = searchMode;
	logPath = pathToLogFile;
	inUtterance = false;

	if(doInit) {
		if(recognitionMode == SphinxHelper::SearchMode::LM) {
			lmPath = pathToSearchFile;
			config = cmd_ln_init(NULL, ps_args(), TRUE,
						 "-hmm", hmmPath.c_str(),
						 "-dict", dictionaryPath.c_str(),
						 "-lm", lmPath.c_str(),
						 "-logfn", logPath.c_str(),
							NULL);
		}
		else if(recognitionMode == SphinxHelper::SearchMode::JSGF) {
			jsgfPath = pathToSearchFile;
			config = cmd_ln_init(NULL, ps_args(), TRUE,
						 "-hmm", hmmPath.c_str(),
						 "-dict", dictionaryPath.c_str(),
						 "-jsgf", jsgfPath.c_str(),
						 "-logfn", logPath.c_str(),
							NULL);
		}

		ps_default_search_args(config);
		ps = ps_init(config);
		if(ps == NULL) {
			Buckey::logError("Unable to initialize PS Decoder!");
			state = SphinxHelper::DecoderState::ERROR;
		}
		else {
			state = SphinxHelper::DecoderState::IDLE;
		}
	}
}

SphinxDecoder::~SphinxDecoder()
{
	state = SphinxHelper::DecoderState::NOT_INITIALIZED;
    ps_free(ps);
    cmd_ln_free_r(config);
}

std::string SphinxDecoder::getName() {
	return name;
}

std::string SphinxDecoder::getHypothesis() {
	if(state == SphinxHelper::DecoderState::IDLE || state == SphinxHelper::DecoderState::NOT_INITIALIZED || state == SphinxHelper::DecoderState::ERROR) {
		Buckey::logWarn("Attempting to get hypothesis from decoder that is not ready! Check to make sure it is not errored out!");
		return "";
	}

	state = SphinxHelper::DecoderState::UTTERANCE_ENDING;
    const char* hyp = ps_get_hyp(ps, NULL);

    if (hyp != NULL) {
    	return std::string(hyp);
    }
    else {
		return "";
    }
}

void SphinxDecoder::startUtterance() {
	if(!(state == SphinxHelper::DecoderState::IDLE || state == SphinxHelper::DecoderState::UTTERANCE_ENDING)) {
		Buckey::logWarn("Attempting to start decoder that is not in the IDLE state! Check to make sure it is initialized!");
		return;
	}

	state = SphinxHelper::DecoderState::UTTERANCE_STARTED;
    if(ps_start_utt(ps) < 0) {
		state = SphinxHelper::DecoderState::ERROR;
        Buckey::logError("Error while starting Utterance for PS Decoder!");
    }
    else {
        ready = true;
        inUtterance = true;
    }
}

void SphinxDecoder::endUtterance() {
	if(state != SphinxHelper::DecoderState::UTTERANCE_STARTED) {
		Buckey::logWarn("Attempted to stop utterance of a decoder that did not start an utterance! Check that you started speech recognition!");
		return;
	}
	state = SphinxHelper::DecoderState::UTTERANCE_ENDING;
    ready = false;
    inUtterance = false;
    ps_end_utt(ps);
}

/// Returns true if speech was detected in the last frame
bool SphinxDecoder::processRawAudio(int16 adbuf[], int32 frameCount) {
    ps_process_raw(ps, adbuf, frameCount, FALSE, FALSE);
    return ps_get_in_speech(ps);
}

cmd_ln_t * SphinxDecoder::getConfig() {
    return config;
}

void SphinxDecoder::addWord(std::string word, std::string phonemes) {
    ps_add_word(ps, word.c_str(), phonemes.c_str(), FALSE);
}

///Return true if the word is in the current dictionary
const bool SphinxDecoder::wordExists(std::string word) {
    return ps_lookup_word(ps, word.c_str()) != NULL;
}

void SphinxDecoder::updateAcousticModel(std::string pathToHMM, bool forceUpdate) {
    hmmPath = pathToHMM;
    if(forceUpdate) {
        reloadDecoder();
    }
}

void SphinxDecoder::updateDictionary(std::string pathToDict, bool forceUpdate) {
    dictionaryPath = pathToDict;
    if(forceUpdate) {
        reloadDecoder();
    }
}

void SphinxDecoder::updateJSGF(std::string pathToJSGF, bool forceUpdate) {
	jsgfPath = pathToJSGF;
	if(forceUpdate) {
        reloadDecoder();
    }
}

void SphinxDecoder::updateLM(std::string pathToLM, bool forceUpdate) {
	lmPath = pathToLM;
	if(forceUpdate) {
        reloadDecoder();
    }
}

void SphinxDecoder::updateLoggingFile(std::string pathToLog, bool forceUpdate) {
	logPath = pathToLog;
	if(forceUpdate) {
        reloadDecoder();
    }
}

void SphinxDecoder::updateSearchMode(SphinxHelper::SearchMode mode, bool forceUpdate) {
	recognitionMode = mode;
	if(forceUpdate) {
        reloadDecoder();
    }
}

/// Reinitializes the decoder after all changes to acoustic model, search mode, dictionary are made
void SphinxDecoder::reloadDecoder() {
    ready = false;

    if(state == SphinxHelper::DecoderState::ERROR) {
		Buckey::logWarn("Attempting to reload an errored out decoder!");
    }

    //bool newDecoder = state == SphinxHelper::DecoderState::NOT_INITIALIZED; // If the decoder wasn't created before, we'll need to make a new one now

    state = SphinxHelper::DecoderState::NOT_INITIALIZED;
    //TODO: Ensure that the dictionary is preserved after reinitializing, current all added words will be removed after reinitializing

    if(recognitionMode == SphinxHelper::SearchMode::LM) {
		//TODO: Use cmd_ln_set_r() instead of making a new config object. This is not a memory leak b/c sphinxbase frees it internally, but it would still have memory and time.
		config = cmd_ln_init(NULL, ps_args(), TRUE,
					 "-hmm", hmmPath.c_str(),
					 "-dict", dictionaryPath.c_str(),
					 "-lm", lmPath.c_str(),
					 "-logfn", logPath.c_str(),
						NULL);
	}
	else if(recognitionMode == SphinxHelper::SearchMode::JSGF) {
		config = cmd_ln_init(NULL, ps_args(), TRUE,
					 "-hmm", hmmPath.c_str(),
					 "-dict", dictionaryPath.c_str(),
					 "-jsgf", jsgfPath.c_str(),
					 "-logfn", logPath.c_str(),
						NULL);
	}

    ps_default_search_args(config);
	ps = ps_init(config);

    /*
    if(newDecoder) { // This is a new decoder, call init
	    ps = ps_init(config);
    }
    else { // This is not a new decoder, call reinit
		ps_reinit(ps, config); // TODO: Fix this. If you pass NULL instead of config it loses the correct search mode.
    }*/

    if(ps == NULL) {
		state = SphinxHelper::DecoderState::ERROR;
        Buckey::logError("Unable to initialize PS Decoder!");
    }
    else {
    	state = SphinxHelper::DecoderState::IDLE;
    }
    ///NOTE: Utterance is not started after reloading!
}

const bool SphinxDecoder::isReady() {
    return ready;
}

const SphinxHelper::DecoderState SphinxDecoder::getState() {
	return state.load();
}

//Getters
std::string SphinxDecoder::getDictionaryPath() {
	return dictionaryPath;
}

std::string SphinxDecoder::getLMPath() {
	return lmPath;
}

std::string SphinxDecoder::getJSGFPath() {
	return jsgfPath;
}

std::string SphinxDecoder::getHMMPath() {
	return hmmPath;
}

std::string SphinxDecoder::getLogPath() {
	return logPath;
}
