#include "SphinxMode.h"
#include "ReplyType.h"
#include "Service.h"
#include "SphinxService.h"

#include <chrono>
#include <thread>

unsigned long SphinxMode::serviceEnableHandler = 0;
unsigned long SphinxMode::serviceDisableHandler = 0;
unsigned long SphinxMode::serviceRegisterHandler = 0;
std::atomic<bool> SphinxMode::sphinxRunning(false);
std::atomic<bool> SphinxMode::instanceSet(false);
std::chrono::high_resolution_clock::time_point SphinxMode::lastToggleTimePoint = std::chrono::high_resolution_clock::now();
SphinxMode * SphinxMode::instance = nullptr;

SphinxMode * SphinxMode::getInstance() {
	if(!instanceSet.load()) {
		instanceSet = true;
		instance = new SphinxMode();
	}
	return instance;
}

SphinxMode::SphinxMode()
{
	name = "pyramid";
	setState(ModeState::STOPPED);
	lastToggleTimePoint = std::chrono::high_resolution_clock::now();
}

void SphinxMode::setupAssetsDir(cppfs::FileHandle aDir) {
	cppfs::FileHandle tempGram = aDir.open("sphinx-mode.gram");
	unique_ptr<ostream> o = tempGram.createOutputStream();

	const char * content = "#JSGF v1.0;\n\
grammar sphinx-mode;\n\
public <command> = <pause> {pause} | <resume> {resume} | <push-to-talk> {push-to-talk} | <toggle> {toggle};\n\
<pause> = pause speech recognition;\n\
<resume> = resume speech recognition;\n\
<toggle> = toggle speech recognition;\n\
<push-to-talk> = ((enable | enter | begin | start) {enable} | (disable | exit | end | stop) {disable}) (push | press) to talk [mode];";

	o->write(content, strlen(content));
	o->flush();
}

void SphinxMode::setupConfigDir(cppfs::FileHandle cDir) {
	//Nothing to setup for config, for now at least.
}

void SphinxMode::start() {
	std::unique_ptr<std::istream> gramInput = assetsDir.open("sphinx-mode.gram").createInputStream();
	std::istream * i = gramInput.release();
	grammar = new DynamicGrammar(i);
	delete i;
	Buckey * b = Buckey::getInstance();
	SphinxMode::serviceEnableHandler = b->addListener(ONSERVICEENABLE, SphinxMode::serviceEnabledEventHandler);
	SphinxMode::serviceDisableHandler = b->addListener(ONSERVICEDISABLE, SphinxMode::serviceDisabledEventHandler);
	SphinxMode::serviceRegisterHandler = b->addListener(ONSERVICEREGISTER, SphinxMode::serviceEventHandler);


	b->addModeToRootGrammar(this, grammar);
	state = ModeState::STARTED;
}

void SphinxMode::stop() {
	Buckey * b = Buckey::getInstance();
    b->unsetListener(ONSERVICEENABLE, SphinxMode::serviceEnableHandler);
	b->unsetListener(ONSERVICEDISABLE, SphinxMode::serviceDisableHandler);
	b->unsetListener(ONSERVICEREGISTER, SphinxMode::serviceRegisterHandler);

	b->removeModeFromRootGrammar(this, grammar);
	delete grammar;
	state = ModeState::STOPPED;
}

void SphinxMode::input(std::string & command, std::vector<std::string> & tags) {
	Buckey * b = Buckey::getInstance();
	std::string & action = *(tags.begin());

	if(sphinxRunning.load() || b->getServiceState("sphinx") == ServiceState::RUNNING) {
			SphinxService * s = ((SphinxService *) (b->getService("sphinx")));
		if(action == "pause") {
			if(s->inPressToSpeak()) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				s->pressToSpeakButtonUp();
            }
			else {
				s->pauseRecognition();
				Buckey::logDebug("Pausing sphinx recognition");
				b->reply("Pausing sphinx speech recognition", ReplyType::CONVERSATION);
			}
		}
		else if (action == "resume") {
            if(s->inPressToSpeak()) {
				s->pressToSpeakButtonDown();
            }
			else {
				s->resumeRecognition();
				Buckey::logDebug("Resuming sphinx recognition");
				b->reply("Resuming sphinx speech recognition", ReplyType::CONVERSATION);
			}
		}
		else if (action == "push-to-talk") {
			std::string & onOff = *(tags.begin() + 1);
			if(onOff == "enable") {
				s->stopRecognition();
				s->startPressToSpeakRecognition();
			}
			else if(onOff == "disable") {
				s->stopRecognition();
				s->startContinuousDeviceRecognition();
			}
		}
		else if (action == "toggle") {
			//See how many milliseconds it has been since the last toggle command has been processed.
			std::chrono::high_resolution_clock::time_point stop = std::chrono::high_resolution_clock::now();
			int millisDiff = std::chrono::duration_cast<std::chrono::milliseconds>(stop - lastToggleTimePoint).count();

			//Received the toggle command but we already received a toggle command less than TOGGLE_DEBOUNCE_TIME milliseconds ago so ignore this command b/c it is probably a repeat
			if(millisDiff < TOGGLE_DEBOUNCE_TIME) {
                return;
			}
			else {
				//Process this toggle command and log the timestamp so we know to ignore repeat commands that come in a few milliseconds
				lastToggleTimePoint = stop;
			}

			if(s->inPressToSpeak()) {
				if(s->pressToSpeakIsPressed()) {
					//std::this_thread::sleep_for(std::chrono::seconds(1));
					s->pressToSpeakButtonUp();
				}
				else {
					s->pressToSpeakButtonDown();
				}
            }
			else {
				if(s->isPaused()) {
					s->resumeRecognition();
					Buckey::logDebug("Resuming sphinx recognition");
					b->reply("Resuming sphinx speech recognition", ReplyType::CONVERSATION);
				}
				else {
					s->pauseRecognition();
					Buckey::logDebug("Pausing sphinx recognition");
					b->reply("Pausing sphinx speech recognition", ReplyType::CONVERSATION);
				}
			}
		}
	}
	else {
		b->reply("You must have the sphinx service enabled to use that command.", ReplyType::CONVERSATION);
	}
}

void SphinxMode::serviceEventHandler(EventData * data, std::atomic<bool> * done) {

}

void SphinxMode::serviceDisabledEventHandler(EventData * data, std::atomic<bool> * done) {
	ServiceControlEventData * d = (ServiceControlEventData *) data;
	if(d->getService()->getName() == "sphinx") {
		sphinxRunning.store(false);
	}
}

void SphinxMode::serviceEnabledEventHandler(EventData * data, std::atomic<bool> * done) {
	ServiceControlEventData * d = (ServiceControlEventData *) data;
	if(d->getService()->getName() == "sphinx") {
		sphinxRunning.store(true);
	}
}

SphinxMode::~SphinxMode()
{
	if(getState() == ModeState::STARTED) {
		delete grammar;
	}
	instanceSet = false;
}
