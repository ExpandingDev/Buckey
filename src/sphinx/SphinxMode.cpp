#include "SphinxMode.h"
#include "ReplyType.h"
#include "Service.h"
#include "SphinxService.h"

#include <syslog.h>

unsigned long SphinxMode::serviceEnableHandler = 0;
unsigned long SphinxMode::serviceDisableHandler = 0;
unsigned long SphinxMode::serviceStopHandler = 0;
unsigned long SphinxMode::serviceRegisterHandler = 0;
std::atomic<bool> SphinxMode::sphinxRunning(false);

std::atomic<bool> SphinxMode::instanceSet(false);
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
	setState(ModeState::DISABLED);
}

void SphinxMode::setupAssetsDir(cppfs::FileHandle aDir) {
	cppfs::FileHandle tempGram = aDir.open("sphinx-mode.gram");
	unique_ptr<ostream> o = tempGram.createOutputStream();

	const char * content = "\
grammar sphinx-mode;\n\
public <command> = <pause> {pause} | <resume> {resume};\
<pause> = pause speech recognition;\
<resume> = resume speech recognition;";

	o->write(content, strlen(content));
	o->flush();
}

void SphinxMode::setupConfigDir(cppfs::FileHandle cDir) {
	//Nothing to setup for config, for now at least.
}

void SphinxMode::enable() {
	std::unique_ptr<std::istream> gramInput = assetsDir.open("sphinx-mode.gram").createInputStream();
	std::istream * i = gramInput.release();
	grammar = new DynamicGrammar(i);
	delete i;

	SphinxMode::serviceEnableHandler = Buckey::getInstance()->addListener(ONSERVICEENABLE, SphinxMode::serviceEnabledEventHandler);
	SphinxMode::serviceDisableHandler = Buckey::getInstance()->addListener(ONSERVICEDISABLE, SphinxMode::serviceDisabledEventHandler);
	SphinxMode::serviceStopHandler = Buckey::getInstance()->addListener(ONSERVICESTOP, SphinxMode::serviceEventHandler);
	SphinxMode::serviceRegisterHandler = Buckey::getInstance()->addListener(ONSERVICEREGISTER, SphinxMode::serviceEventHandler);


	Buckey::getInstance()->addModeToRootGrammar(this, grammar);
	state = ModeState::ENABLED;
}

void SphinxMode::disable() {
    Buckey::getInstance()->unsetListener(ONSERVICEENABLE, SphinxMode::serviceEnableHandler);
	Buckey::getInstance()->unsetListener(ONSERVICEDISABLE, SphinxMode::serviceDisableHandler);
	Buckey::getInstance()->unsetListener(ONSERVICESTOP, SphinxMode::serviceStopHandler);
	Buckey::getInstance()->unsetListener(ONSERVICEREGISTER, SphinxMode::serviceRegisterHandler);

	Buckey::getInstance()->removeModeFromRootGrammar(this, grammar);
	delete grammar;
	state = ModeState::DISABLED;
}

void SphinxMode::input(std::string & command, std::vector<std::string> & tags) {
	Buckey * b = Buckey::getInstance();
	std::string & action = *(tags.begin());

	if(sphinxRunning.load() || b->getServiceState("sphinx") == ServiceState::RUNNING) {
		if(action == "pause") {
			((SphinxService *) (b->getService("sphinx")))->pauseRecognition();
			syslog(LOG_DEBUG, "Pausing sphinx recognition");
			b->reply("Pausing sphinx speech recognition", ReplyType::CONVERSATION);
		}
		else if (action == "resume") {
			((SphinxService *) (b->getService("sphinx")))->resumeRecognition();
			syslog(LOG_DEBUG, "Resuming sphinx recognition");
			b->reply("Resuming sphinx speech recognition", ReplyType::CONVERSATION);
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
	if(getState() == ModeState::ENABLED) {
		delete grammar;
	}
	instanceSet = false;
}
