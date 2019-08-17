#include "CoreMode.h"
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

unsigned long CoreMode::serviceRegisterHandler = 0;
unsigned long CoreMode::serviceEnableHandler = 0;
unsigned long CoreMode::serviceDisableHandler = 0;

CoreMode::CoreMode()
{
	name = "core";
	setState(ModeState::STOPPED);
}

CoreMode::~CoreMode()
{
	if(getState() == ModeState::STARTED) {
		delete CoreMode::grammar;
	}
	instanceSet = false;
}

CoreMode * CoreMode::getInstance() {
	if(!instanceSet) {
		instance = new CoreMode();
		instanceSet = true;
	}
	return instance;
}

void CoreMode::start() {
	std::unique_ptr<istream> uni = assetsDir.open("core.gram").createInputStream();
	std::istream * i = uni.release();
	grammar = new DynamicGrammar(i);
	delete i;

	state = ModeState::STARTED;
	finishHandler = Buckey::getInstance()->addListener(ONFINISHINIT, CoreMode::initFinished);
	registerHandler = Buckey::getInstance()->addListener(ONMODEREGISTER, CoreMode::updateGrammar);
	disableHandler = Buckey::getInstance()->addListener(ONMODEDISABLE, CoreMode::updateGrammar);
	enableHandler = Buckey::getInstance()->addListener(ONMODEENABLE, CoreMode::updateGrammar);

	serviceEnableHandler = Buckey::getInstance()->addListener(ONSERVICEENABLE, CoreMode::updateGrammar);
	serviceDisableHandler = Buckey::getInstance()->addListener(ONSERVICEDISABLE, CoreMode::updateGrammar);
	serviceRegisterHandler = Buckey::getInstance()->addListener(ONSERVICEREGISTER, CoreMode::updateGrammar);
}


void CoreMode::stop() {
	Buckey::getInstance()->removeModeFromRootGrammar(this, grammar);
	delete grammar;
	state = ModeState::STOPPED;
	Buckey::getInstance()->unsetListener(ONFINISHINIT, finishHandler);
	Buckey::getInstance()->unsetListener(ONMODEENABLE, enableHandler);
	Buckey::getInstance()->unsetListener(ONMODEDISABLE, disableHandler);
	Buckey::getInstance()->unsetListener(ONMODEREGISTER, registerHandler);

	Buckey::getInstance()->unsetListener(ONSERVICEENABLE, serviceEnableHandler);
	Buckey::getInstance()->unsetListener(ONSERVICEDISABLE, serviceDisableHandler);
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
<mode-command> = (<enable-mode> | <disable-mode> | <stop-mode> | <start-mode> | <reload>) mode {mode} <$modeList>;\n\
<service-command> = (<enable-service> | <disable-service> | <stop-service> | <start-service> | <reload>) service {service} <$serviceList>;\n\
<enable-service> = enable {enable};\n\
<stop-service> = stop {stop};\n\
<start-service> = start {start};\n\
<disable-service> = disable {disable};\n\
<list-command> = <list> (enabled {enabled} | disabled {disabled} | (started | running) {started} | stopped {stopped} | [all | available ] {all}) (modes {modes} | services {services});\n\
<list> = (list | show | tell me) {list};\n\
<enable-mode> = (enable | enter | turn on | activate) {enable};\n\
<disable-mode> = (disable | leave | turn off | deactivate | exit) {disable};\n\
<stop-mode> = stop {stop};\n\
<start-mode> = start {start};\n\
<reload> = (reload | restart | reboot) {reload};\n\
<quit> = (quit | exit | goodbye) {quit};";

	o->write(content, strlen(content));
	o->flush();
}

///TODO: Implement started and stopped listings and also start and stop modes and services.
void CoreMode::input(std::string & command, std::vector<std::string> & tags) {
	Buckey * b = Buckey::getInstance();
	std::string & action = *(tags.begin());

	if(action == "quit") {
		b->reply("Goodbye", ReplyType::CONVERSATION);
		b->requestStop();
		return;
	}

	std::string & target = *(tags.begin()+2);
	if(action == "start") {
		if(*(tags.begin()+1) == "mode") {
			b->startMode(target.c_str());
			b->reply("Started " + target + " mode", ReplyType::STATUS);
		} else if(*(tags.begin()+1) == "service") {
			b->startService(target);
			b->reply("Started " + target + " service", ReplyType::STATUS);
		}
	}
	else if(action == "stop") {
		if(*(tags.begin()+1) == "mode") {
			if(target != "core") {
				b->stopMode(target.c_str());
				b->reply("Stopped " + target + " mode", ReplyType::STATUS);
			}
			else {
				b->reply("I'm sorry but you cannot stop the core mode.", ReplyType::CONVERSATION);
			}
		} else if(*(tags.begin()+1) == "service") {
			b->stopService(target);
			b->reply("Stopped " + target + " service", ReplyType::STATUS);
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
	else if(action == "enable") {
		if(*(tags.begin()+1) == "mode") {
			b->enableMode(target.c_str());
			b->reply("Enabled " + target + " mode", ReplyType::STATUS);
		} else if(*(tags.begin()+1) == "service") {
			b->enableService(target);
			b->reply("Enabled " + target + " service", ReplyType::STATUS);
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
				std::string m;
				if(out.size() == 0) {
					m = "No modes are enabled";
				}
				else {
					m = "Modes " + StringHelper::concatenateStringVector(out, ' ') + " are enabled";
				}
				b->reply(m, ReplyType::CONVERSATION);
			}
			else if(target == "services") {
				std::vector<std::string> out;
				b->listEnabledServices(out);
				std::string m;
				if(out.size() == 0) {
					m = "No services are enabled";
				}
				else {
					m = "Services " + StringHelper::concatenateStringVector(out, ' ') + " are enabled";
				}
				b->reply(m, ReplyType::CONVERSATION);
			}
		}
		else if(*(tags.begin()+1) == "disabled") {
			if(target == "modes") {
				std::vector<std::string> out;
				b->listDisabledModes(out);
				std::string m ;
				if(out.size() == 0) {
					m = "No modes are disabled.";
				}
				else {
					m = "Modes " + StringHelper::concatenateStringVector(out, ' ') + " are disabled";
				}
				b->reply(m, ReplyType::CONVERSATION);
			}
			else if(target == "services") {
				std::vector<std::string> out;
				b->listDisabledServices(out);
				std::string m;
				if(out.size() == 0) {
					m = "No services are disabled";
				}
				else {
					m = "Services " + StringHelper::concatenateStringVector(out, ' ') + " are disabled";
				}
				b->reply(m, ReplyType::CONVERSATION);
			}
		}
		else if(*(tags.begin()+1) == "started") {
			if(target == "modes") {
				std::vector<std::string> out;
				b->listModesInState(out, ModeState::STARTED);
				std::string m;
				if(out.size() == 0) {
					m = "No modes have been started";
				}
				else {
					m = "Modes " + StringHelper::concatenateStringVector(out, ' ') + " are started";
				}
				b->reply(m, ReplyType::CONVERSATION);
			}
			else if(target == "services") {
				std::vector<std::string> out;
				b->listServicesInState(out, ServiceState::RUNNING);
				std::string m;
				if(out.size() == 0) {
					m = "No services are running";
				}
				else {
					m = "Services " + StringHelper::concatenateStringVector(out, ' ') + " are running";
				}
				b->reply(m, ReplyType::CONVERSATION);
			}
		}
		else if(*(tags.begin()+1) == "stopped") {
			if(target == "modes") {
				std::vector<std::string> out;
				b->listModesInState(out, ModeState::STOPPED);
				std::string m;
				if(out.size() == 0) {
					m ="No modes are stopped";
				}
				else {
					m = "Modes " + StringHelper::concatenateStringVector(out, ' ') + " are stopped";
				}
				b->reply(m, ReplyType::CONVERSATION);
			}
			else if(target == "services") {
				std::vector<std::string> out;
				b->listServicesInState(out, ServiceState::STOPPED);
				std::string m;
				if(out.size() == 0) {
					m ="No services are stopped";
				}
				else {
					m = "Services " + StringHelper::concatenateStringVector(out, ' ') + " are stopped";
				}
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
	if(getInstance()->getState() == ModeState::STARTED) { // Don't update the grammar if the event was triggered by disabling the core mode!
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
			SphinxService * s = ((SphinxService *) b->getService("sphinx"));
			s->setJSGF(b->getRootGrammar());
			s->updateSearchMode(SphinxHelper::JSGF);
			s->applyUpdates();
		}
	}
	else {
		Buckey::logWarn("Core Mode Called when not enabled");
	}
	done->store(true);
}
