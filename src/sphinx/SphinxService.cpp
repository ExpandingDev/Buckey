#include "SphinxService.h"
#include "ReplyType.h"
#include "Buckey.h"
#include "HypothesisEventData.h"
#include <chrono>

using namespace std::chrono;

SphinxService * SphinxService::instance;
std::atomic<bool> SphinxService::instanceSet(false);
unsigned long SphinxService::onEnterPromptEventHandlerID;
unsigned long SphinxService::onConversationEndEventHandlerID;

SphinxService * SphinxService::getInstance() {
	if(!instanceSet) {
		instance = new SphinxService();
		instanceSet.store(true);
	}
	return instance;
}

std::string SphinxService::getName() const {
	return "sphinx";
}

void SphinxService::setupAssets(cppfs::FileHandle aDir) {
	cppfs::FileHandle confirmGram = aDir.open("confirm.gram");

	if(!confirmGram.writeFile("#JSGF v1.0;\n\
grammar confirm;\n\
public <command> = <positive> {yes} | <deny> {no};\n\
<positive> = yes | yeah | ok | positive | confirm | of course | please do | yes sir;\n\
<deny> = no | deny | reject | [please] don't | nope | do not;")) {
		///TODO: Error setting up confirm.gram
		Buckey::logError("SphinxService failed to create confirm.gram asset file.");
	}
}

void SphinxService::setupConfig(cppfs::FileHandle cDir) {
	YAML::Node n;
	n["logfile"] = "/dev/null";
	n["max-frame-size"] = 2048;
	n["hmm-dir"] = "/usr/local/share/pocketsphinx/model/en-us/en-us";
	n["dict-dir"] = "/usr/local/share/pocketsphinx/model/en-us/cmudict-en-us.dict";
//	n["speech-device"] = "default";
	n["default-lm"] = "/usr/local/share/pocketsphinx/model/en-us/en-us.lm.bin";
	n["max-decoders"] = 2;
	n["samples-per-second"] = 16000; // Taken from libsphinxad ad.h is usually 16000
	cppfs::FileHandle cFile = cDir.open("decoder.conf");
	YAML::Emitter e;
	e << n;
	cFile.writeFile(e.c_str());
}

SphinxService::SphinxService() : manageThreadRunning(false), currentDecoderIndex(0), inUtterance(false), endLoop(false), paused(false), pressToSpeakMode(false), pressToSpeakPressed(false) {

}

SphinxService::~SphinxService()
{
    endLoop.store(true);

    //usleep(100); // TODO: Windows portability
    if(manageThreadRunning.load() || recognizerLoop.joinable()) {
		recognizerLoop.join();
    }

    for(std::thread & t : miscThreads) {
        t.join();
    }

    for(SphinxDecoder * sd : decoders) {
        delete sd;
    }

    instanceSet.store(false);
}

void SphinxService::start() {
	setState(ServiceState::STARTING);
	endLoop.store(false);
    voiceDetected.store(false);
    recognizing.store(false);
    isRecording.store(false);
    paused.store(false);
    pressToSpeakMode.store(false);
    pressToSpeakPressed.store(false);
    config = YAML::LoadFile(configDir.open("decoder.conf").path());
    maxDecoders = config["max-decoders"].as<unsigned int>();
    deviceName = "";
    if(config["speech-device"]) {
    	deviceName = config["speech-device"].as<std::string>();
    }

	for(unsigned short i = 0; i < maxDecoders; i++) {
		SphinxDecoder * sd = new SphinxDecoder("base-grammar", config["default-lm"].as<std::string>(), SphinxHelper::SearchMode::LM, config["hmm-dir"].as<std::string>(), config["dict-dir"].as<std::string>(), config["logfile"].as<std::string>(), true);
		decoders.push_back(sd);
	}

	startPressToSpeakRecognition(deviceName);
    ///TODO: Set listeners

    onEnterPromptEventHandlerID = Buckey::getInstance()->addListener("onEnterPromptEvent", onEnterPromptEventHandler);
    onConversationEndEventHandlerID = Buckey::getInstance()->addListener("onExitPromptEvent", onConversationEndEventHandler);

    setState(ServiceState::RUNNING);
}

void SphinxService::stop() {
	if(getState() == ServiceState::RUNNING) {
		Buckey::getInstance()->unsetListener("onEnterPromptEvent", onEnterPromptEventHandlerID);
		Buckey::getInstance()->unsetListener("onExitPromptEvent", onConversationEndEventHandlerID);
		stopRecognition();
	}
	setState(ServiceState::STOPPED);
}

void SphinxService::stopRecognition() {
	Buckey::logInfo("Received request to stop recognition.");
    endLoop.store(true);
    if(manageThreadRunning.load()) {
		recognizerLoop.join();
    }
}

void SphinxService::pauseRecognition() {
	paused.store(true);
	triggerEvents(ON_PAUSE, new EventData());
}

void SphinxService::resumeRecognition() {
	triggerEvents(ON_RESUME, new EventData());
	paused.store(false);
}

bool SphinxService::isRecordingToFile() {
	return isRecording;
}

void SphinxService::stopRecordingToFile() {
	isRecording.store(false);
	fflush(recordingFileHandle);
	fclose(recordingFileHandle);
}

bool SphinxService::startRecordingToFile() {
	if(recordingFileHandle != NULL) {
		isRecording.store(false);
		return true;
	}
	else {
		return false;
	}
}

void SphinxService::onEnterPromptEventHandler(EventData * data, std::atomic<bool> * done) {
	PromptEventData * d = (PromptEventData *) data;
	Buckey::logInfo("enter prompt event handler");
	if(d->getType() == "confirm") {
        SphinxService::getInstance()->updateJSGFPath(SphinxService::getInstance()->assetsDir.open("confirm.gram").path());
        SphinxService::getInstance()->applyUpdates();
        Buckey::logInfo("switched grammars");
	}
	done->store(true);
}

void SphinxService::onConversationEndEventHandler(EventData * data, std::atomic<bool> * done) {
	Buckey::logInfo("exit prompt event handler");
	SphinxService::getInstance()->setJSGF(Buckey::getInstance()->getRootGrammar());
	SphinxService::getInstance()->applyUpdates();
	done->store(true);
}

bool SphinxService::recordAudioToFile(std::string pathToAudioFile) {
	recordingFile = pathToAudioFile;
	recordingFileHandle = fopen(pathToAudioFile.c_str(), "wb");
	//TODO: Implement proper file closing when stop recording method is called. Currently stops recording when it reaches the end of an audio file, but does not close it when the stop function is called.
	if(recordingFileHandle == NULL) {
		Buckey::logError("Unable to open raw audio file to write recorded audio! " + pathToAudioFile);
		return false;
	}
	else {
		isRecording.store(true);
		return true;
	}
}

///Static loop that runs during non-continuous/push to speak recognition
void SphinxService::manageNonContinuousDecoders(SphinxService * sr) {
	//Lock all mutexes and flags first
	sr->manageThreadRunning.store(true);
	sr->updateLock.lock();

	Buckey * b = Buckey::getInstance();

    ad_rec_t *ad = nullptr; // Audio source
    int16 adbuf[sr->config["max-frame-size"].as<int>()]; //buffer that audio frames are copied into
    int32 frameCount = 0; // Number of frames read into the adbuf

    sr->endLoop.store(false);
    sr->inUtterance.store(false);
    sr->voiceDetected.store(false); // Reset this as its used to keep track of state

    Buckey::logInfo("Decoder management thread started");

    if (sr->source == SphinxHelper::DEVICE) {
        Buckey::logInfo("Opening audio device for recognition");
        if(sr->deviceName == "") {
	    if ((ad = ad_open_sps(sr->config["samples-per-second"].as<int>())) == NULL) { // DEFAULT_SAMPLES_PER_SEC is taken from libsphinxad ad.h is usually 16000
	            Buckey::logError("Failed to open audio device");
	            sr->recognizing.store(false);
            	    return;
	    }
	}
	else if((ad = ad_open_dev(sr->deviceName.c_str(), sr->config["samples-per-second"].as<int>())) == NULL) { // DEFAULT_SAMPLES_PER_SEC is taken from libsphinxad ad.h is usually 16000
            Buckey::logError("Failed to open audio device");
            sr->recognizing.store(false);
            return;
        }
    }
    else {
		Buckey::logWarn("Cannot open noncontinuous decoding for FILE!");
		sr->recognizing.store(false);
		return;
    }

    sr->currentDecoderIndex.store(0);
    Buckey::logInfo("Starting Utterances...");

    // Start up all of the utterances
    for(SphinxDecoder * sd : sr->decoders) {
        sd->startUtterance();
    }
    Buckey::logInfo("Done starting Utterances.");

    while(sr->decoders[sr->currentDecoderIndex]->state == SphinxHelper::DecoderState::NOT_INITIALIZED) {
		//Wait until the decoder is ready
    }

    sr->recognizing.store(false);

    sr->triggerEvents(ON_READY, new EventData());
    Buckey::getInstance()->reply("Sphinx Speech Recognition Ready", ReplyType::CONSOLE);
	sr->updateLock.unlock();

	sr->triggerEvents(ON_SERVICE_READY, new EventData());

    while(!b->isKilled() && !sr->endLoop.load()) {

		while(!b->isKilled() && !sr->endLoop.load() && !sr->pressToSpeakPressed.load()) {
			//Wait until press to speak is pressed or we have to stop
		}

		//Make sure we didn't exit the loop because we have to stop
		if(b->isKilled() || sr->endLoop.load()) {
			break;
		}

		sr->recognizing.store(true);

		auto start = high_resolution_clock::now();
		//Start recording from audio device
        if (ad_start_rec(ad) < 0) {
            Buckey::logError("Failed to start recording");
            sr->recognizing.store(false);
            break;
        }
        auto stop = high_resolution_clock::now();
    	auto duration = duration_cast<milliseconds>(stop - start);
    	std::cout << "Time to start recording: " << duration.count() << std::endl;

        // Read from the audio buffer while press to speak is pressed
		while(sr->pressToSpeakPressed.load() && !sr->endLoop.load() && !b->isKilled()) {
			frameCount = ad_read(ad, adbuf, AUDIO_FRAME_SIZE);

			//Check to make sure we got frames from the audio device
			if(frameCount < 0 ) {
				if(sr->source == SphinxHelper::DEVICE) {
					Buckey::logError("Failed to read from audio device for sphinx recognizer!");
					/// TODO: Maybe fail a bit more gracefully
					sr->killThreads();
					break;
				}
			}
			else if(sr->isRecording) {
				fwrite(adbuf, sizeof(int16), frameCount, sr->recordingFileHandle);
				fflush(sr->recordingFileHandle);
			}

			// Check to make sure our current decoder has not errored out
			if(sr->decoders[sr->currentDecoderIndex]->state == SphinxHelper::DecoderState::ERROR) {
				Buckey::logError("Decoder is errored out! Trying next decoder...");
				bool found = false;
				for(unsigned short i = sr->currentDecoderIndex; i < sr->maxDecoders - 1; i++) {
					if(sr->decoders[sr->currentDecoderIndex]->isReady()) {
						sr->currentDecoderIndex.store(sr->currentDecoderIndex + i);
						found = true;
						break;
					}
				}
				if(!found) {
					Buckey::logError("No more good decoders to use! Stopping speech recognition!");
					sr->killThreads();
					break;
				}
			}
/*
			// Check to make sure our current decoder is still ready to process speech (make sure the utterance has been started)
			if(sr->decoders[sr->currentDecoderIndex]->state != SphinxHelper::DecoderState::UTTERANCE_STARTED) {
				sr->decoders[sr->currentDecoderIndex]->startUtterance();
			}
*/
			// Process the frames
			sr->voiceDetected.store(sr->decoders[sr->currentDecoderIndex]->processRawAudio(adbuf, frameCount));

			// Silence to speech transition
			// Trigger onSpeechStart
			if(sr->voiceDetected && !sr->inUtterance) {
				sr->triggerEvents(ON_START_SPEECH, new EventData());
				sr->inUtterance.store(true);
				b->playSoundEffect(SoundEffects::READY, false);
			}
		}

		//Make sure we didn't exit the loop because we have to stop
		if(b->isKilled() || sr->endLoop.load()) {
			break;
		}

		//Stop recording
		ad_stop_rec(ad);

        //End and get hypothesis
		sr->decoderIndexLock.lock();
		sr->inUtterance.store(false);
		sr->recognizing.store(false);
		sr->decoders[sr->currentDecoderIndex]->ready = false;
		sr->miscThreads.push_back(std::thread(endAndGetHypothesis, sr, sr->decoders[sr->currentDecoderIndex]));
		sr->decoderIndexLock.unlock();

		//Refresh decoders
		sr->decoderIndexLock.lock();
		for(unsigned short i = 0; i < sr->maxDecoders; i++) {
			if(sr->decoders[i]->isReady()) {
				sr->currentDecoderIndex.store(i);
			}
		}
		sr->decoderIndexLock.unlock();

    }


    //Close the device audio source
	Buckey::logInfo("Closing audio device");
	ad_close(ad);

    sr->recognizing.store(false);
    sr->pressToSpeakMode.store(false);
    sr->pressToSpeakPressed.store(false);
    sr->manageThreadRunning.store(false);
}

bool SphinxService::inPressToSpeak() {
	return pressToSpeakMode.load();
}

bool SphinxService::pressToSpeakIsPressed() {
	return pressToSpeakPressed.load();
}

bool SphinxService::isPaused() {
	return paused.load();
}

/// Static loop that runs during continuous recognition
void SphinxService::manageContinuousDecoders(SphinxService * sr) {
	Buckey * b = Buckey::getInstance();
	sr->manageThreadRunning.store(true);
	sr->updateLock.lock();
	Buckey::logInfo("Decoder management thread started");
	sr->endLoop.store(false);
    ad_rec_t *ad = nullptr; // Audio source

    int16 adbuf[sr->config["max-frame-size"].as<int>()]; //buffer that audio frames are copied into
    int32 frameCount = 0; // Number of frames read into the adbuf

    sr->inUtterance.store(false);

    sr->voiceDetected.store(false); // Reset this as its used to keep track of state

    if(sr->source == SphinxHelper::FILE) {
        Buckey::logInfo("Opening file for recognition");
        // TODO: Implement opening the file, reliant upon specifying the args passed during startFileRecognition
    }
    else if (sr->source == SphinxHelper::DEVICE) {
        Buckey::logInfo("Opening audio device for recognition");
        // TODO: Use ad_open_dev without pocketsphinx's terrible configuration functions
        //if ((ad = ad_open_dev(NULL,(int) cmd_ln_float32_r(sr->decoders[0]->getConfig(),"-samprate"))) == NULL) {
        Buckey::logInfo("Opening audio device for recognition");
        if(sr->deviceName == "") {
	    if ((ad = ad_open_sps(sr->config["samples-per-second"].as<int>())) == NULL) { // DEFAULT_SAMPLES_PER_SEC is taken from libsphinxad ad.h is usually 16000
	            Buckey::logError("Failed to open audio device");
	            sr->recognizing.store(false);
            	    return;
	    }
	}
	else if((ad = ad_open_dev(sr->deviceName.c_str(), sr->config["samples-per-second"].as<int>())) == NULL) { // DEFAULT_SAMPLES_PER_SEC is taken from libsphinxad ad.h is usually 16000
            Buckey::logError("Failed to open audio device");
            sr->recognizing.store(false);
            return;
        }

        if (ad_start_rec(ad) < 0) {
            Buckey::logError("Failed to start recording\n");
            sr->recognizing.store(false);
            return;
        }
    }

    sr->currentDecoderIndex.store(0);
    Buckey::logInfo("Starting Utterances...");
    // Start up all of the utterances
    for(SphinxDecoder * sd : sr->decoders) {
        sd->startUtterance();
    }
    Buckey::logInfo("Done starting Utterances.");

    while(sr->decoders[sr->currentDecoderIndex]->state == SphinxHelper::DecoderState::NOT_INITIALIZED) {
		//Wait until the decoder is ready
    }

    sr->recognizing.store(true);
    sr->triggerEvents(ON_READY, new EventData());
    Buckey::getInstance()->reply("Sphinx Speech Recognition Ready", ReplyType::CONSOLE);
	sr->updateLock.unlock();

	sr->triggerEvents(ON_SERVICE_READY, new EventData());

    while(!sr->endLoop.load()) {

        // Read from the audio buffer
        if(sr->source == SphinxHelper::DEVICE) {
			if(sr->paused.load()) {
				sr->decoderIndexLock.lock();
				sr->decoders[sr->currentDecoderIndex]->endUtterance();
				sr->inUtterance.store(false);
				sr->decoders[sr->currentDecoderIndex]->startUtterance();
				sr->decoderIndexLock.unlock();
			}
			while(sr->paused.load() && !sr->endLoop) {
				//Wait until not paused, but continue reading frames so that we only read current frames when we resume recognition
				frameCount = ad_read(ad, adbuf, AUDIO_FRAME_SIZE);
    		}
    		if(sr->endLoop) {
				break;
    		}

    		frameCount = ad_read(ad, adbuf, AUDIO_FRAME_SIZE);
        }
        else if (sr->source == SphinxHelper::FILE) {
        	///NOTE: Pausing and resuming recognition only works from a device, not a file.
            frameCount = fread(adbuf, sizeof(int16), AUDIO_FRAME_SIZE, sr->sourceFile);
        }

        if(frameCount < 0 ) {
            if(sr->source == SphinxHelper::DEVICE) {
                Buckey::logError("Failed to read from audio device for sphinx recognizer!");
                // TODO: Maybe fail a bit more gracefully
                sr->killThreads();
                exit(-1);
            }
        }
        else if(sr->isRecording) {
			fwrite(adbuf, sizeof(int16), frameCount, sr->recordingFileHandle);
			fflush(sr->recordingFileHandle);
		}

        // Check to make sure our current decoder has not errored out
        if(sr->decoders[sr->currentDecoderIndex]->state == SphinxHelper::DecoderState::ERROR) {
		Buckey::logError("Decoder is errored out! Trying next decoder...");
			bool found = false;
			for(unsigned short i = sr->currentDecoderIndex; i < sr->maxDecoders - 1; i++) {
				if(sr->decoders[sr->currentDecoderIndex]->isReady()) {
					sr->currentDecoderIndex.store(sr->currentDecoderIndex + i);
					found = true;
					break;
				}
			}
			if(!found) {
				Buckey::logError("No more good decoders to use! Stopping speech recognition!");
				sr->killThreads();
				return;
			}
		}


        if(frameCount <= 0 && sr->source == SphinxHelper::FILE) {
                Buckey::logInfo("Reached end of audio file, stopping speech recognition...");
                if(sr->inUtterance) { // Reached end of file before end of speech, so stop recognition and get the hypothesis
					sr->decoderIndexLock.lock();
                    sr->triggerEvents(ON_END_SPEECH, new EventData()); // TODO: Add event data
                    sr->inUtterance.store(false);
                    sr->decoders[sr->currentDecoderIndex]->ready = false;
                    sr->miscThreads.push_back(std::thread(endAndGetHypothesis, sr, sr->decoders[sr->currentDecoderIndex]));
					sr->decoderIndexLock.unlock();
					if(sr->isRecording) {
						sr->isRecording.store(false);
						fflush(sr->recordingFileHandle);
						fclose(sr->recordingFileHandle);
					}
                    break;
                }
        }

        // Process the frames
        sr->voiceDetected.store(sr->decoders[sr->currentDecoderIndex]->processRawAudio(adbuf, frameCount));

        // Silence to speech transition
        // Trigger onSpeechStart
        if(sr->voiceDetected && !sr->inUtterance) {
            sr->triggerEvents(ON_START_SPEECH, new EventData());
            sr->inUtterance.store(true);
			b->playSoundEffect(SoundEffects::READY, false);
        }

        //Speech to silence transition
        //Trigger onSpeechEnd
        //And get hypothesis
        if(!sr->voiceDetected && sr->inUtterance) {

	    sr->decoderIndexLock.lock();
            sr->triggerEvents(ON_END_SPEECH, new EventData()); //TODO: Add event data
            sr->inUtterance.store(false);
            sr->decoders[sr->currentDecoderIndex]->ready = false;
            sr->miscThreads.push_back(std::thread(endAndGetHypothesis, sr, sr->decoders[sr->currentDecoderIndex]));
	    sr->decoderIndexLock.unlock();

            usleep(100); // TODO: Windows portability

	    sr->decoderIndexLock.lock();
            for(unsigned short i = 0; i < sr->maxDecoders; i++) {
                if(sr->decoders[i]->isReady()) {
                    sr->currentDecoderIndex.store(i);
                }
            }
            sr->decoderIndexLock.unlock();
        }
    }

    if(sr->source == SphinxHelper::FILE) {
        fclose(sr->sourceFile);
    }

    //Close the device audio source
    if (sr->source == SphinxHelper::DEVICE) {
        Buckey::logInfo("Closing audio device");
        ad_close(ad);
    }

    sr->recognizing.store(false);
    sr->manageThreadRunning.store(false);
}

void SphinxService::startContinuousDeviceRecognition(std::string device) {
	Buckey::logInfo("Starting device recognition");
	if(!recognizing) {
		source = SphinxHelper::DEVICE;
		if(device != "") {
			deviceName = device;
		}
		else {
//			deviceName = config["speech-device"].as<std::string>();
			device = "";
		}

		if(recognizerLoop.joinable()) {
			recognizerLoop.join();
		}

		pressToSpeakMode.store(false);
		recognizerLoop = std::thread(manageContinuousDecoders, this);
	}
	else {
		Buckey::logWarn("Calling start device recognition while recognition already in progress!");
	}
}

void SphinxService::startPressToSpeakRecognition(std::string device) {
	Buckey::logInfo("Starting press to speak device recognition");
	if(!recognizing) {
		source = SphinxHelper::DEVICE;

		if(device != "") {
			deviceName = device;
		}
		else {
//			deviceName = config["speech-device"].as<std::string>();
			deviceName = "";
		}

		if(recognizerLoop.joinable()) {
			recognizerLoop.join();
		}
		pressToSpeakMode.store(true);
		recognizerLoop = std::thread(manageNonContinuousDecoders, this);
	}
	else {
		Buckey::logWarn("Calling start device recognition while recognition already in progress!");
	}
}

void SphinxService::pressToSpeakButtonDown() {
	pressToSpeakPressed.store(true);
}

void SphinxService::pressToSpeakButtonUp() {
	pressToSpeakPressed.store(false);
}

void SphinxService::endAndGetHypothesis(SphinxService * sr, SphinxDecoder * sd) {
    sd->endUtterance();
    std::string hyp = sd->getHypothesis();
    if(hyp != "") { // Ignore false alarms
		Buckey::logInfo("Got hypothesis: " + hyp);
		Buckey::getInstance()->playSoundEffect(SoundEffects::OK, false);
        sr->triggerEvents(ON_HYPOTHESIS, new HypothesisEventData(hyp));
        Buckey::getInstance()->passInput(hyp);
    }
    sd->startUtterance();
}

void SphinxService::startFileRecognition(std::string pathToFile) {
    if((sourceFile = fopen(pathToFile.c_str(), "rb")) == NULL) {
        Buckey::logError("Unable to open file for speech recognition: " + pathToFile);
        stopRecognition();
        for(std::thread & t : miscThreads) {
            t.join();
        }

        for(SphinxDecoder * sd : decoders) {
            delete sd;
        }
        exit(-1);
    }
    else {
        source = SphinxHelper::FILE;
        Buckey::logInfo("Opened file for speech recognition: " + pathToFile);
    }
    recognizerLoop = std::thread(manageContinuousDecoders, this);
}

bool SphinxService::wordExists(std::string word) {
	bool res = false;
	bool found = false;
	while(!found) {
		for(unsigned short i = 0; i < maxDecoders - 1; i++) {
			if(decoders[currentDecoderIndex]->isReady()) {
				res = decoders[currentDecoderIndex]->wordExists(word);
				found = true;
				break;
			}
		}
	}
	return res;
}

void SphinxService::addWord(std::string word, std::string phones) {
	for(unsigned short i = 0; i < maxDecoders - 1; i++) {
		if(decoders[currentDecoderIndex]->getState() != SphinxHelper::DecoderState::ERROR && decoders[currentDecoderIndex]->getState() != SphinxHelper::DecoderState::NOT_INITIALIZED) {
			decoders[currentDecoderIndex]->addWord(word, phones);
		}
		else {
			Buckey::logWarn("Unable to add word " + word + " to decoder because it was not initialized or errored out!");
		}
	}
}

void SphinxService::updateDictionary(std::string pathToDictionary) {
    for(SphinxDecoder * sd : decoders) {
        sd->updateDictionary(pathToDictionary, false);
    }
}

void SphinxService::updateAcousticModel(std::string pathToHMM) {
    for(SphinxDecoder * sd : decoders) {
        sd->updateAcousticModel(pathToHMM, false);
    }
}

void SphinxService::updateJSGFPath(std::string pathToJSGF) {
    for(SphinxDecoder * sd : decoders) {
        sd->updateJSGF(pathToJSGF, false);
    }
}

void SphinxService::updateLMPath(std::string pathToLM) {
    for(SphinxDecoder * sd : decoders) {
        sd->updateLM(pathToLM, false);
    }
}

void SphinxService::updateLogPath(std::string pathToLog) {
    for(SphinxDecoder * sd : decoders) {
        sd->updateLoggingFile(pathToLog, false);
    }
}

void SphinxService::updateSearchMode(SphinxHelper::SearchMode mode) {
    for(SphinxDecoder * sd : decoders) {
        sd->updateSearchMode(mode, false);
    }
}

void SphinxService::setJSGF(Grammar * g) {
	///TODO: Save Grammar to a temp file, pass the path of the temp file on to the sphinx decoders
    cppfs::FileHandle t = Buckey::getInstance()->getTempFile(".gram");
    t.writeFile("#JSGF V1.0;\n" + g->getText());
    updateJSGFPath(t.path());
}

/// Applies previous updates and also initializes decoders if there weren't already when this object was constructed.
void SphinxService::applyUpdates() {
	updateLock.lock();
	Buckey::logInfo("Starting to apply updates.");
	auto start = high_resolution_clock::now();
    if(isRecognizing()) { // Decoders are in use so reload the ones not in use
		Buckey::logInfo("Attempting to update while recognizing...");
		unsigned short k = decoders.size();
		unsigned short decodersDone[k];
		unsigned short decoderDoneCount = 0;
		while(decoderDoneCount != decoders.size()) {
			for(unsigned short i = 0; i < decoders.size(); i++) {
				unsigned short * c = std::find(decodersDone, decodersDone+k, i);
				if(c != decodersDone+k) {
					// This one was already updated, let it go
				}
				else {
					//Not updated yet
					if(i == currentDecoderIndex) {
						if(!inUtterance) {
							if(decoderDoneCount > 0) {
								decoderIndexLock.lock();
								currentDecoderIndex.store(decodersDone[0]);
								decoderIndexLock.unlock();
							}
						}
					}
					else {
						if(decoders[i]->getState() == SphinxHelper::DecoderState::UTTERANCE_STARTED) {
							decoders[i]->reloadDecoder();
							decoders[i]->startUtterance();
							decodersDone[decoderDoneCount] = i;
							decoderDoneCount++;
						}
					}
				}
			}
		}
    }
    else { // Decoders are not in use so restart them all now
    	Buckey::logInfo("Applying updates while not recognizing...");
		for(unsigned short i = 0; i < decoders.size(); i++) {
			decoders[i]->reloadDecoder();
			decoders[i]->startUtterance();

			if(i == 0) { // Select the first decoder that we update so it is ready ASAP
				decoderIndexLock.lock();
				currentDecoderIndex.store(0);
				decoderIndexLock.unlock();
			}
		}

    }
    Buckey::logInfo("Decoder Update Applied");
    updateLock.unlock();
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);
    std::cout << "Time to apply decoder update: " << duration.count() << std::endl;
}

bool SphinxService::isRecognizing() {
    return recognizing;
}

bool SphinxService::voiceFound() {
    return voiceDetected.load();
}

SphinxDecoder * SphinxService::getDecoder(unsigned short decoderIndex) {
    return decoders[decoderIndex];
}

void SphinxService::addOnSpeechStart(void(*handler)(EventData *, std::atomic<bool> *)) {
    addListener(ON_START_SPEECH, handler);
}

void SphinxService::addOnSpeechEnd(void(*handler)(EventData *, std::atomic<bool> *)) {
    addListener(ON_END_SPEECH, handler);
}

void SphinxService::addOnHypothesis(void(*handler)(EventData *, std::atomic<bool> *)) {
    addListener(ON_HYPOTHESIS, handler);
}

void SphinxService::clearSpeechStartListeners() {
	clearListeners(ON_START_SPEECH);
}

void SphinxService::clearSpeechEndListeners() {
	clearListeners(ON_END_SPEECH);
}

void SphinxService::clearOnHypothesisListeners() {
	clearListeners(ON_HYPOTHESIS);
}

void SphinxService::clearOnPauseListeners() {
	clearListeners(ON_PAUSE);
}

void SphinxService::clearOnResumeListeners() {
	clearListeners(ON_RESUME);
}
