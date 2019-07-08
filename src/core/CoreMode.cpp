#include "CoreMode.h"
#include "syslog.h"
#include "iostream"
#include "StringHelper.h"
#include "Buckey.h"
#include "MimicTTSService.h"
#include "SphinxService.h"
#include "alternativeset.h"

using namespace std;
using namespace cppfs;

CoreMode * CoreMode::instance;
DynamicGrammar * CoreMode::grammar;
bool CoreMode::instanceSet = false;
unsigned long CoreMode::finishHandler = 0;
unsigned long CoreMode::registerHandler = 0;
unsigned long CoreMode::enableHandler = 0;
unsigned long CoreMode::disableHandler = 0;

unsigned long CoreMode::serviceStopHandler = 0;
unsigned long CoreMode::serviceRegisterHandler = 0;
unsigned long CoreMode::serviceEnableHandler = 0;
unsigned long CoreMode::serviceDisableHandler = 0;

CoreMode::CoreMode()
{
	name = "core";
	setState(ModeState::DISABLED);
}

CoreMode::~CoreMode()
{
	delete CoreMode::grammar;
	instanceSet = false;
}

CoreMode * CoreMode::getInstance() {
	if(!instanceSet) {
		instance = new CoreMode();
		instanceSet = true;
	}
	return instance;
}

void CoreMode::enable() {
	std::unique_ptr<istream> uni = assetsDir.open("core.gram").createInputStream();
	std::istream * i = uni.release();
	grammar = new DynamicGrammar(i);
	delete i;

	state = ModeState::ENABLED;
	finishHandler = Buckey::getInstance()->addListener(ONFINISHINIT, CoreMode::initFinished);
	registerHandler = Buckey::getInstance()->addListener(ONMODEREGISTER, CoreMode::updateGrammar);
	disableHandler = Buckey::getInstance()->addListener(ONMODEDISABLE, CoreMode::updateGrammar);
	enableHandler = Buckey::getInstance()->addListener(ONMODEENABLE, CoreMode::updateGrammar);

	serviceEnableHandler = Buckey::getInstance()->addListener(ONSERVICEENABLE, CoreMode::updateGrammar);
	serviceDisableHandler = Buckey::getInstance()->addListener(ONSERVICEDISABLE, CoreMode::updateGrammar);
	serviceStopHandler = Buckey::getInstance()->addListener(ONSERVICESTOP, CoreMode::updateGrammar);
	serviceRegisterHandler = Buckey::getInstance()->addListener(ONSERVICEREGISTER, CoreMode::updateGrammar);
}


void CoreMode::disable() {
	Buckey::getInstance()->removeModeFromRootGrammar(this, grammar);
	delete grammar;
	state = ModeState::DISABLED;
	Buckey::getInstance()->unsetListener(ONFINISHINIT, finishHandler);
	Buckey::getInstance()->unsetListener(ONMODEENABLE, enableHandler);
	Buckey::getInstance()->unsetListener(ONMODEDISABLE, disableHandler);
	Buckey::getInstance()->unsetListener(ONMODEREGISTER, registerHandler);

	Buckey::getInstance()->unsetListener(ONSERVICEENABLE, serviceEnableHandler);
	Buckey::getInstance()->unsetListener(ONSERVICEDISABLE, serviceDisableHandler);
	Buckey::getInstance()->unsetListener(ONSERVICESTOP, serviceStopHandler);
	Buckey::getInstance()->unsetListener(ONSERVICEREGISTER, serviceRegisterHandler);
}

void CoreMode::setupConfigDir(FileHandle cDir) {

}

void CoreMode::setupAssetsDir(FileHandle aDir) {
	FileHandle tempGram = aDir.open("core.gram");
	unique_ptr<ostream> o = tempGram.createOutputStream();

	const char * content = "\
grammar core;\n\
public <command> = <quit> | <mode-command> | <service-command> | <list-command>;\n\
<mode-command> = (<enable-mode> | <disable-mode> | <reload>) mode {mode} <$modeList>;\n\
<service-command> = (<enable-service> | <disable-service> | <reload> | stop {stop} ) service {service} <$serviceList>;\n\
<enable-service> = enable {enable};\n\
<disable-service> = disable {disable};\n\
<list-command> = <list> (enabled {enabled} | disabled {disabled} | [all | available ] {all}) (modes {modes} | services {services});\n\
<list> = (list | show | tell me) {list};\n\
<enable-mode> = (enable | enter | turn on | activate) {enable};\n\
<disable-mode> = (disable | leave | turn off | deactivate | exit) {disable};\n\
<reload> = (reload | restart | reboot) {reload};\n\
<quit> = (quit | exit | goodbye) {quit};";

	o->write(content, strlen(content));
	o->flush();
}

void CoreMode::input(std::string & command, std::vector<std::string> & tags) {
	Buckey * b = Buckey::getInstance();
	std::string & action = *(tags.begin());

	if(action == "quit") {
		b->reply("Goodbye", ReplyType::CONVERSATION);
		b->requestStop();
		return;
	}

	std::string & target = *(tags.begin()+2);

	if(action == "enable") {
		if(*(tags.begin()+1) == "mode") {
			b->enableMode(target.c_str());
			b->reply("Enabled " + target + " mode", ReplyType::STATUS);
		} else if(*(tags.begin()+1) == "service") {
			b->enableService(target);
			b->reply("Enabled " + target + " service", ReplyType::STATUS);
		}
	}
	else if(action == "reload") {
		if(*(tags.begin()+1) == "mode") {
			b->reloadMode(target.c_str());
			b->reply("Reloaded " + target + " mode", ReplyType::STATUS);
		} else if(*(tags.begin()+1) == "service") {
			b->reloadService(target);
			b->reply("Reloaded " + target + " service", ReplyType::STATUS);
		}
	}
	else if(action == "disable") {
		if(*(tags.begin()+1) == "mode") {
			if(target != "core") {
				b->disableMode(target.c_str());
				b->reply("Disabled " + target + " mode", ReplyType::STATUS);
			}
			else {
				b->reply("I'm sorry but you cannot disable the core mode.", ReplyType::CONVERSATION);
			}
		} else if(*(tags.begin()+1) == "service") {
			b->disableService(target);
			b->reply("Disabled " + target + " service", ReplyType::STATUS);
		}
	}
	else if(action == "list") {
		if(*(tags.begin()+1) == "enabled") {
			if(target == "modes") {
				std::vector<std::string> out;
				b->listEnabledModes(out);
				std::string m = "Modes " + StringHelper::concatenateStringVector(out, ' ') + " are enabled";
				b->reply(m, ReplyType::CONVERSATION);
			}
			else if(target == "services") {
				std::vector<std::string> out;
				b->listEnabledServices(out);
				std::string m = "Services " + StringHelper::concatenateStringVector(out, ' ') + " are enabled";
				b->reply(m, ReplyType::CONVERSATION);
			}
		}
		else if(*(tags.begin()+1) == "disabled") {
			if(target == "modes") {
				std::vector<std::string> out;
				b->listModesInState(out, ModeState::DISABLED);
				std::string m = "Modes " + StringHelper::concatenateStringVector(out, ' ') + " are disabled";
				b->reply(m, ReplyType::CONVERSATION);
			}
			else if(target == "services") {
				std::vector<std::string> out;
				b->listServicesInState(out, ServiceState::DISABLED);
				std::string m = "Services " + StringHelper::concatenateStringVector(out, ' ') + " are disabled";
				b->reply(m, ReplyType::CONVERSATION);
			}
		}
		else if(*(tags.begin()+1) == "all") {
			if(target == "modes") {
				std::vector<std::string> out;
				b->listModes(out);
				std::string m = "Modes " + StringHelper::concatenateStringVector(out, ' ') + " are registered";
				b->reply(m, ReplyType::CONVERSATION);
			}
			else if(target == "services") {
				std::vector<std::string> out;
				b->listServices(out);
				std::string m = "Services " + StringHelper::concatenateStringVector(out, ' ') + " are registered";
				b->reply(m, ReplyType::CONVERSATION);
			}
		}
	}

}

void CoreMode::initFinished(EventData * data, std::atomic<bool> * done) {
	Buckey * b = Buckey::getInstance();
	if(Buckey::getInstance()->getServiceState("mimic") == ServiceState::RUNNING) {
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("I'm sorry but you cannot disable the core mode");
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("OK");
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Yes");
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("No");
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Doing so now");
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Alright");
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Hello");
		((MimicTTSService *) b->getService("mimic"))->prepareSpeech("My name is buckey McAllister I am a personal assistant");

		vector<string> temp;
		b->listModes(temp);
		for(string s : temp) {
			((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Enabled " + s + " mode");
			((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Disabled " + s + " mode");
			((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Reloaded " + s + " mode");
		}
		temp.clear();
		b->listServices(temp);
		for(string s : temp) {
			((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Enabled " + s + " service");
			((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Disabled " + s + " service");
			((MimicTTSService *) b->getService("mimic"))->prepareSpeech("Reloaded " + s + " service");
		}

	}

	if(b->getServiceState("sphinx") == ServiceState::RUNNING) {
		((SphinxService *) b->getService("sphinx"))->pauseRecognition();
	}

	updateGrammar(data, done);

	if(b->getServiceState("sphinx") == ServiceState::RUNNING) {
		((SphinxService *) b->getService("sphinx"))->resumeRecognition();
	}

	b->playSoundEffect(SoundEffects::ATTENTION, false);
}

void CoreMode::updateGrammar(EventData * data, std::atomic<bool> * done) {
	if(getInstance()->getState() == ModeState::ENABLED) { // Don't update the grammar if the event was triggered by disabling the core mode!
		Buckey * b = Buckey::getInstance();

		shared_ptr<AlternativeSet> modeList = shared_ptr<AlternativeSet>(new AlternativeSet());
		shared_ptr<AlternativeSet> serviceList = shared_ptr<AlternativeSet>(new AlternativeSet());

		vector<string> temp;
		b->listModes(temp);
		for(string s : temp) {
			modeList->addChild(shared_ptr<Tag>(new Tag(shared_ptr<Token>(new Token(s)), s)));
		}
		temp.clear();

		b->listServices(temp);
		for(string s : temp) {
			serviceList->addChild(shared_ptr<Tag>(new Tag(shared_ptr<Token>(new Token(s)), s)));
		}

		grammar->setVariable("modeList", modeList);
		grammar->setVariable("serviceList", serviceList);

		b->addModeToRootGrammar(instance, grammar);
		if(b->getServiceState("sphinx") == ServiceState::RUNNING) {
			((SphinxService *) b->getService("sphinx"))->setJSGF(b->getRootGrammar());
			((SphinxService *) b->getService("sphinx"))->updateSearchMode(SphinxHelper::JSGF);
			((SphinxService *) b->getService("sphinx"))->applyUpdates();
		}
	}
	else {
		syslog(LOG_DEBUG, "Called when not enabled");
	}
	done->store(true);
}
