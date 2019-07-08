#include "SphinxService.h"
#include "ReplyType.h"
#include "Buckey.h"
#include "HypothesisEventData.h"

SphinxService * SphinxService::instance;
std::atomic<bool> SphinxService::instanceSet(false);

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

}

void SphinxService::setupConfig(cppfs::FileHandle cDir) {
	YAML::Node n;
	n["logfile"] = "/dev/null";
	n["max-frame-size"] = 2048;
	n["hmm-dir"] = "/usr/local/share/pocketsphinx/model/en-us/en-us";
	n["dict-dir"] = "/usr/local/share/pocketsphinx/model/en-us/cmudict-en-us.dict";
	n["speech-device"] = "default";
	n["default-lm"] = "/usr/local/share/pocketsphinx/model/en-us/en-us.lm.bin";
	n["max-decoders"] = 2;
	n["samples-per-second"] = 16000; // Taken from libsphinxad ad.h is usually 16000
	cppfs::FileHandle cFile = cDir.open("decoder.conf");
	YAML::Emitter e;
	e << n;
	cFile.writeFile(e.c_str());
}

SphinxService::SphinxService() : manageThreadRunning(false), currentDecoderIndex(0), inUtterance(false) {

}

SphinxService::~SphinxService()
{
    endLoop.store(true);
    usleep(100); // TODO: Windows portability
    if(manageThreadRunning) {
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

void SphinxService::enable() {
	setState(ServiceState::STARTING);
	endLoop.store(false);
    voiceDetected.store(false);
    recognizing.store(false);
    isRecording.store(false);
    paused.store(false);
    config = YAML::LoadFile(configDir.open("decoder.conf").path());
    maxDecoders = config["max-decoders"].as<unsigned int>();
    deviceName = config["speech-device"].as<std::string>();

	for(unsigned short i = 0; i < maxDecoders; i++) {
		SphinxDecoder * sd = new SphinxDecoder("base-grammar", config["default-lm"].as<std::string>(), SphinxHelper::SearchMode::LM, config["hmm-dir"].as<std::string>(), config["dict-dir"].as<std::string>(), config["logfile"].as<std::string>(), true);
		decoders.push_back(sd);
	}

	startDeviceRecognition(config["speech-device"].as<std::string>());
    //TODO: Set listeners

    setState(ServiceState::RUNNING);
}

void SphinxService::disable() {
	if(getState() == ServiceState::RUNNING) {
		stop();
	}
	setState(ServiceState::DISABLED);
	//TODO: Unset listeners
}

void SphinxService::stop() {
	stopRecognition();
	setState(ServiceState::STOPPED);
	//TODO: Unset listeners
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

bool SphinxService::recordAudioToFile(std::string pathToAudioFile) {
	recordingFile = pathToAudioFile;
	recordingFileHandle = fopen(pathToAudioFile.c_str(), "wb");
	//TODO: Implement proper file closing when stop recording method is called. Currently stops recording when it reaches the end of an audio file, but does not close it when the stop function is called.
	if(recordingFileHandle == NULL) {
		syslog(LOG_ERR, "Unable to open raw audio file to write recorded audio! %s", pathToAudioFile.c_str());
		return false;
	}
	else {
		isRecording.store(true);
		return true;
	}
}

/// Static loop that runs during recognition
void SphinxService::manageDecoders(SphinxService * sr) {
	Buckey * b = Buckey::getInstance();
	sr->manageThreadRunning.store(true);
	sr->updateLock.lock();
	syslog(LOG_INFO, "Decoder management thread started");
	sr->endLoop.store(false);
    ad_rec_t *ad = nullptr; // Audio source

    int16 adbuf[sr->config["max-frame-size"].as<int>()]; //buffer that audio frames are copied into
    int32 frameCount = 0; // Number of frames read into the adbuf

    sr->inUtterance.store(false);

    sr->voiceDetected.store(false); // Reset this as its used to keep track of state

    if(sr->source == SphinxHelper::FILE) {
        syslog(LOG_INFO, "Opening file for recognition");
        // TODO: Implement opening the file, reliant upon specifying the args passed during startFileRecognition
    }
    else if (sr->source == SphinxHelper::DEVICE) {
        syslog(LOG_INFO, "Opening audio device for recognition");
        // TODO: Use ad_open_dev without pocketsphinx's terrible configuration functions
        //if ((ad = ad_open_dev(NULL,(int) cmd_ln_float32_r(sr->decoders[0]->getConfig(),"-samprate"))) == NULL) {
        if((ad = ad_open_dev(sr->deviceName.c_str(), sr->config["samples-per-second"].as<int>())) == NULL) { // DEFAULT_SAMPLES_PER_SEC is taken from libsphinxad ad.h is usually 16000
            syslog(LOG_ERR, "Failed to open audio device\n");
            sr->recognizing.store(false);
            return;
        }

        if (ad_start_rec(ad) < 0) {
            syslog(LOG_ERR, "Failed to start recording\n");
            sr->recognizing.store(false);
            return;
        }
    }

    sr->currentDecoderIndex.store(0);
    syslog(LOG_INFO, "Starting Utterances... \n");
    // Start up all of the utterances
    for(SphinxDecoder * sd : sr->decoders) {
        sd->startUtterance();
    }
    syslog(LOG_INFO, "Done starting Utterances.\n");

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
			while(sr->paused && !sr->endLoop) {
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
                syslog(LOG_ERR, "Failed to read from audio device for sphinx recognizer!");
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
		syslog(LOG_ERR, "Decoder is errored out! Trying next decoder...");
			bool found = false;
			for(unsigned short i = sr->currentDecoderIndex; i < sr->maxDecoders - 1; i++) {
				if(sr->decoders[sr->currentDecoderIndex]->isReady()) {
					sr->currentDecoderIndex.store(sr->currentDecoderIndex + i);
					found = true;
					break;
				}
			}
			if(!found) {
				syslog(LOG_ERR, "No more good decoders to use! Stopping speech recognition!");
				sr->killThreads();
				return;
			}
		}


        if(frameCount <= 0 && sr->source == SphinxHelper::FILE) {
                syslog(LOG_INFO, "Reached end of audio file, stopping speech recognition...");
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
        syslog(LOG_INFO, "Closing audio device");
        ad_close(ad);
    }

    sr->recognizing.store(false);
    sr->manageThreadRunning.store(false);
}

void SphinxService::stopRecognition() {
	syslog(LOG_INFO, "Received request to stop recognition.");
    endLoop.store(true);
}

void SphinxService::startDeviceRecognition(std::string device) {
	syslog(LOG_INFO, "Starting device recognition");
	if(!recognizing) {
		source = SphinxHelper::DEVICE;
		if(device != "") {
			deviceName = device;
		}
		else {
			deviceName = config["speech-device"].as<std::string>();
		}

		if(recognizerLoop.joinable()) {
			recognizerLoop.join();
		}

		recognizerLoop = std::thread(manageDecoders, this);
	}
	else {
		syslog(LOG_WARNING, "Calling start device recognition while recognition already in progress!");
	}
}

void SphinxService::endAndGetHypothesis(SphinxService * sr, SphinxDecoder * sd) {
    sd->endUtterance();
    std::string hyp = sd->getHypothesis();
    if(hyp != "") { // Ignore false alarms
		syslog(LOG_INFO, "Got hypothesis: %s", hyp.c_str());
		Buckey::getInstance()->playSoundEffect(SoundEffects::OK, false);
        sr->triggerEvents(ON_HYPOTHESIS, new HypothesisEventData(hyp));
        Buckey::getInstance()->passInput(hyp);
    }
    sd->startUtterance();
}

void SphinxService::startFileRecognition(std::string pathToFile) {
    if((sourceFile = fopen(pathToFile.c_str(), "rb")) == NULL) {
        syslog(LOG_ERR, "Unable to open file for speech recognition: %s", pathToFile.c_str());
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
        syslog(LOG_INFO, "Opened file for speech recognition: %s", pathToFile.c_str());
    }
    recognizerLoop = std::thread(manageDecoders, this);
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
			syslog(LOG_WARNING, "Unable to add word %s to decoder because it was not initialized or errored out!", word.c_str());
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
	syslog(LOG_INFO, "Starting to apply updates.");
    if(isRecognizing()) { // Decoders are in use so reload the ones not in use
		syslog(LOG_INFO, "Attempting to update while recognizing...");
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
    	syslog(LOG_INFO, "Applying updates while not recognizing...");
		for(unsigned short i = 0; i < decoders.size(); i++) {
			decoders[i]->reloadDecoder();
			//decoders[i]->startUtterance();

			if(i == 0) { // Select the first decoder that we update so it is ready ASAP
				decoderIndexLock.lock();
				currentDecoderIndex.store(0);
				decoderIndexLock.unlock();
			}
		}

    }
    syslog(LOG_INFO, "Decoder Update Applied");
    updateLock.unlock();
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
