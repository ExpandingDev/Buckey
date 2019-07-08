#include "MimicTTSService.h"
#include "Buckey.h"
#include "OutputEventData.h"
#include <SpeechPreparedEventData.h>
#include <AsyncSpeechRequestEventData.h>
#include "SDL2/SDL.h"
#include "SDL2/SDL_mixer.h"

std::atomic<bool> MimicTTSService::instanceSet(false);
MimicTTSService * MimicTTSService::instance;

MimicTTSService::MimicTTSService() : TTSService()
{
	error = 0;
	addListener(ASYNC_SPEECH_REQUEST, doAsyncRequest);
}

MimicTTSService * MimicTTSService::getInstance() {
	if(!instanceSet) {
		instance = new MimicTTSService();
		instanceSet.store(true);
	}
	return instance;
}

/// Reads the list of prepared audio files from the prepared audio list
void MimicTTSService::loadPreparedAudioList() {
	preparedAudioList = assetsDir.open("preparedAudio.list");

	if(!preparedAudioList.isFile()) {
		syslog(LOG_WARNING, "Mimic TTS prepared audio list %s not found, making one!", preparedAudioList.fileName().c_str());
		preparedAudioList.writeFile("# MIMIC TTS Service Prepared Audio Files List\n");
	}

	unsigned int index = 0;

	std::unique_ptr<std::istream> in = preparedAudioList.createInputStream();
	char buff[500];
	while(in->getline(buff, 500)) {
		if(buff[0] == '#') {
			continue; // Comment line, skip it
		}
		else {
			std::string fileName = std::to_string(index) + ".wav";
			preparedAudio.push_back(std::pair<std::string,std::string>(std::string(buff), fileName ));
			index++;
		}
	}

	syslog(LOG_INFO, "%lu Prepared Audio Files added to list for Mimic TTS", preparedAudio.size());
	lastAudioIndex = index;
}

void MimicTTSService::setupConfig(cppfs::FileHandle cDir) {
	cppfs::FileHandle configFile = cDir.open("mimic.conf");

    std::unique_ptr<std::ostream> out = configFile.createOutputStream();

    YAML::Node n;
    n["voice-file"] = "/home/tyler/Documents/Programming/Libraries/mimic/voices/mycroft_voice_4.0.flitevox";
    YAML::Emitter emit;
    emit << n;

    (*out) << emit.c_str();
    out->flush();
}

void MimicTTSService::setupAssets(cppfs::FileHandle aDir) {
	cppfs::FileHandle pAudioList = assetsDir.open("preparedAudio.list");
	pAudioList.writeFile("# MIMIC TTS Service Prepared Audio Files List\n");
}


/**
  * This function is a synchronous function that prepares the words and triggers the onSpeechPrepared event when done.
  */
void MimicTTSService::prepareSpeech(std::string words) {
	std::transform(words.begin(), words.end(), words.begin(), ::tolower);
	for(std::pair<std::string, std::string> p : preparedAudio) {
		if(p.first == words) {
			return; // This audio already exists
		}
	}

	std::string fileName = std::to_string(lastAudioIndex) + ".wav";

	cst_wave * w = mimic_text_to_wave(words.c_str(), voice);
	cst_wave_save_riff(w, assetsDir.open(fileName).path().c_str());
	lastAudioIndex++;
    std::unique_ptr<std::ostream> o = preparedAudioList.createOutputStream(std::ios_base::app);
    words += "\n";
	o->write(words.c_str(), words.length());
	o->flush();

    preparedAudio.push_back(std::pair<std::string, std::string>(words, fileName));
	delete w;
	triggerEvents(ON_MIMIC_AUDIO_PREPARED, new SpeechPreparedEventData(words, fileName));
}

/**
  * This function is a synchronous function that prepares all of the words in the list and does not produce any events.
  */
void MimicTTSService::prepareList(std::vector<std::string> & wordList) {

	for(std::string words : wordList) {
		std::transform(words.begin(), words.end(), words.begin(), ::tolower);
		for(std::pair<std::string, std::string> p : preparedAudio) {
			if(p.first == words) {
				return; // This audio already exists
			}
		}

		std::string fileName = std::to_string(lastAudioIndex) + ".wav";

		cst_wave * w = mimic_text_to_wave(words.c_str(), voice);
		cst_wave_save_riff(w, assetsDir.open(fileName).path().c_str());
		lastAudioIndex++;
		std::unique_ptr<std::ostream> o = preparedAudioList.createOutputStream(std::ios_base::app);
		o->write(words.c_str(), words.length());
		o->flush();

		preparedAudio.push_back(std::pair<std::string, std::string>(words, fileName));
		delete w;
	}

}

/// Speaks speech that has been prepared and returns true. Returns false if the specified words don't exist
bool MimicTTSService::speakPreparedSpeech(std::string words) {
	for(std::pair<std::string, std::string> p : preparedAudio) {
		if(p.first == words) {
			//cppfs::FilePath fp(assetsDir.path());
			std::string fname = assetsDir.open(p.second).path();

			Mix_Chunk *sample;
			sample=Mix_LoadWAV(fname.c_str());
			if(!sample) {
					printf("Mix_LoadWAV: %s\n", Mix_GetError());// handle error
			}

			while(currentlySpeaking) {
				//Wait until an opening occurs. This is mainly for async speech calls.
			}

			currentlySpeaking.store(true);
			triggerEvents(ON_SPEECH_START, new EventData());

			int channel = Mix_PlayChannel(-1, sample, false);
			if(channel == -1) {
				///ERROR
				return false;
			}
			Mix_Volume(channel, 128);
			while(Mix_Playing(channel)) {
				//Wait
			}
			Mix_FreeChunk(sample);

			currentlySpeaking.store(false);
			triggerEvents(ON_SPEECH_END, new EventData());
			return true;
		}
	}
	return false;
}

/**
  * Speaks the specified words whether they are prepared or not
  */
int MimicTTSService::speak(std::string words) {
	if(!stopRequest && state == ServiceState::RUNNING) {

		history.push_back(words);

		//First check to see if these words are prepared already and speak them
		if(speakPreparedSpeech(words)) {
			return 0;
		}

		//If not prepared yet, synthesize them and speak them
		while(currentlySpeaking) {
			//Wait until an opening occurs. This is mainly for async speech calls.
		}
		currentlySpeaking.store(true);
		triggerEvents(ON_SPEECH_START, new EventData());

		//error = mimic_text_to_speech(words.c_str(), voice, "play", &durs);

		cst_wave * w = mimic_text_to_wave(words.c_str(), voice);
		Mix_Chunk * sample = new Mix_Chunk();
		sample->alen = (unsigned int) w->num_samples * sizeof(short); //Don't touch this, it just works....
		sample->abuf = (unsigned char *) w->samples;
		sample->volume = 128;
		sample->allocated = 0;
		int channel = Mix_PlayChannel(-1, sample, false);
		if(channel == -1) {
			return false;
		}
		while(Mix_Playing(channel)) {
			//Idle
		}
		Mix_FreeChunk(sample);
		delete_wave(w);

		currentlySpeaking.store(false);
		triggerEvents(ON_SPEECH_END, new EventData());
		return error;
	}
	return 0;
}

/// Returns the last spoken words. Useful if something needs repeated.
std::string MimicTTSService::lastSaid() {
	return history[history.size() - 1];
}

/// Returns the words spoken at the specified index in the history. Useful if something needs repeated.
std::string MimicTTSService::wordsSaidAt(unsigned long index) {
	return history[index];
}

/// Returns the number of items in the history. Useful if something needs repeated.
unsigned long MimicTTSService::getHistorySize() {
	return history.size();
}

/// This has not been implemented and probably never will be
void MimicTTSService::asyncSpeak(std::string words) {
	if(!stopRequest && state == ServiceState::RUNNING) {
		triggerEvents(ASYNC_SPEECH_REQUEST, new AsyncSpeechRequestEventData(words, this));
	}
}

bool MimicTTSService::stopSpeaking() {
	stopRequest.store(true);
	while(currentlySpeaking) {
		//Wait for it to end
	}
	return true;
}

bool MimicTTSService::isSpeaking() {
	return currentlySpeaking;
}

void MimicTTSService::addOnSpeechPrepared(void (*handler)(EventData *, std::atomic<bool> *)) {
	addListener(ON_MIMIC_AUDIO_PREPARED, handler);
}

void MimicTTSService::enable() {
	if(getState() != ServiceState::RUNNING && getState() != ServiceState::STARTING) {
    	setState(ServiceState::STARTING);
    	syslog(LOG_INFO, "Initializing Mimic core...");
		mimic_init();

		mimic_add_lang("eng", usenglish_init, cmu_lex_init);

		// Default voice file
		std::string v = "/home/tyler/Documents/Programming/Libraries/mimic1/voices/mycroft_voice_4.0.flitevox";

		if(MimicTTSService::config["voice-file"]) {
			v = MimicTTSService::config["voice-file"].as<std::string>();
		}

		loadPreparedAudioList();

		voice = mimic_voice_load(v.c_str());
		stopRequest.store(false);
		setState(ServiceState::RUNNING);
        onOutputHandle = Buckey::getInstance()->addListener(ONOUTPUT, handleOutputs);
	}
}

void MimicTTSService::handleOutputs(EventData * data, std::atomic<bool> * done) {
    OutputEventData * d = ((OutputEventData *) data);
    if(d->getType() == ReplyType::CONVERSATION) {
		getInstance()->asyncSpeak(d->getMessage());
    }
    else if(d->getType() == ReplyType::STATUS) {
	    getInstance()->asyncSpeak(d->getMessage());
    }
	done->store(true);
}

void MimicTTSService::disable() {
	if(state == ServiceState::RUNNING) {
		stopSpeaking();
		Buckey::getInstance()->unsetListener(ONOUTPUT, onOutputHandle);
		Service::setState(ServiceState::DISABLED);
		mimic_exit();
	}
	else if(state == ServiceState::STARTING) {
		//Wait until startup is finished
		while(state==ServiceState::STARTING) {

		}
		if(state != ServiceState::ERROR) {
			stopSpeaking();
			Service::setState(ServiceState::DISABLED);
			mimic_exit();
		}
		Buckey::getInstance()->unsetListener(ONOUTPUT, onOutputHandle);
	}
	else if(state == ServiceState::STOPPED) {
		Service::setState(ServiceState::DISABLED);
		Buckey::getInstance()->unsetListener(ONOUTPUT, onOutputHandle);
		mimic_exit();
	}
}

void MimicTTSService::stop() {
	if(state == ServiceState::RUNNING) {
		stopSpeaking();
		Service::setState(ServiceState::STOPPED);
		mimic_exit();
	}
	else if(state == ServiceState::STARTING) {
		//Wait until startup is finished
		while(state==ServiceState::STARTING) {

		}
		if(state != ServiceState::ERROR) {
			stopSpeaking();
			Service::setState(ServiceState::STOPPED);
			mimic_exit();
		}
	}
	Buckey::getInstance()->unsetListener(ONOUTPUT, onOutputHandle);
}

std::string MimicTTSService::getStatusMessage() const {
	return Service::generateNormalStatusMessage(getState(), getName());
}

std::string MimicTTSService::getName() const {
	return "mimic";
}

void MimicTTSService::doAsyncRequest(EventData * data, std::atomic<bool> * done) {
	AsyncSpeechRequestEventData * req = (AsyncSpeechRequestEventData *) data;
	req->tts->speak(req->text);
	done->store(true);
}

MimicTTSService::~MimicTTSService()
{
	//delete voice;
	 instanceSet.store(false);
	mimic_exit();

	///NOTE: See above note about versions and the naming of these functions.
	//mimic_core_exit();
}
