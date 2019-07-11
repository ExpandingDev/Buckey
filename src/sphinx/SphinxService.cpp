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
	n["num-decoders"] = 2;
	n["samples-per-second"] = 16000; // Taken from libsphinxad ad.h is usually 16000
	n["press-to-speak"] = false;
	cppfs::FileHandle cFile = cDir.open("decoder.conf");
	YAML::Emitter e;
	e << n;
	cFile.writeFile(e.c_str());
}

SphinxService::SphinxService() {

}

SphinxService::~SphinxService()
{
    instanceSet.store(false);
}

void SphinxService::createDefaultSearches(ps_decoder_t * p) {

	//(Wake word) Search mode name: buckey
	if(ps_set_keyphrase(p, "buckey", "buckey") == -1) {
		///TODO: Error
	}

	//Search mode name: confirm
	if(ps_set_jsgf_file(p, "confirm", assetsDir.open("confirm.gram").path().c_str()) == -1) {
		///TODO: Error handling
	}
}

SphinxDecoder * SphinxService::createDecoder(std::string name) {
	cmd_ln_t * config = cmd_ln_init(NULL, ps_args(), TRUE,
						 "-hmm", defaultHMMPath.c_str(),
						 "-dict", defaultDictionaryPath.c_str(),
						 "-lm", defaultLMPath.c_str(),
						 "-logfn", defaultLogPath.c_str(), NULL);
	ps_default_search_args(config);
	ps_decoder_t * p = ps_init(config);
	if(p == NULL) {
		///TODO: Error
	}
	createDefaultSearches(p);
	SphinxDecoder * d = new SphinxDecoder(name, p);
	return d;
}

void SphinxService::enable() {
	setState(ServiceState::STARTING);

	//Set defaults from the config file
	defaultLogPath = config["logfile"].as<std::string>();
	defaultHMMPath = config["hmm-dir"].as<std::string>();
	defaultLMPath = config["default-lm"].as<std::string>();
	defaultDictionaryPath = config["dict-dir"].as<std::string>();

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

	setState(ServiceState::STOPPED);
	//TODO: Unset listeners
}
