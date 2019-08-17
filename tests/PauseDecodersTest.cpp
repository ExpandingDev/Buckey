#include "SphinxDecoderManager.h"
#include "EventSource.h"
#include "EventData.h"
#include "Buckey.h"
#include "SphinxHelper.h"

#include "yaml-cpp/yaml.h"

#include <string>
#include <vector>
#include <atomic>
#include <iostream>

using namespace std;

atomic<bool> done;
atomic<bool> requestPause;

void onHypothesis(EventData * d, atomic<bool> * done) {
	HypothesisEventData * h = (HypothesisEventData *) d;
	cout << "Hyp: " << h->getHypothesis() << endl;
	if(h->getHypothesis() == "one") {
		requestPause.store(true);
	}
	done->store(true);
}

int main() {
	done.store(false);
	requestPause.store(false);

	YAML::Node config;
	Buckey::applyDefaultSphinxConfig(config);

	SphinxDecoderManager manager(config, false);

	manager.addOnHypothesis(onHypothesis);
	manager.updateJSGFPath("tests/digits.gram");
	manager.updateSearchMode(SphinxHelper::SearchMode::JSGF);
	manager.applyUpdates();

	manager.startDeviceRecognition("plughw:CARD=Headset,DEV=0");

	cout << "Starting up" << endl;
	std::string s;
	while (!done) {
		if(requestPause) {
			cout << "Stopping" << endl;
			manager.stopRecognition();
			cin >> s;
			requestPause.store(false);
			cout << "Starting" << endl;
			manager.startDeviceRecognition();
		}
	}
}
