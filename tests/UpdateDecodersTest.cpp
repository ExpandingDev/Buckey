#include "SphinxDecoderManager.h"
#include "Buckey.h"
#include "yaml-cpp/yaml.h"
#include <iostream>
#include <string>

using namespace std;
SphinxDecoderManager * sr;

void printHypothesis(EventData * ed, atomic<bool> * done) {
    HypothesisEventData * h = (HypothesisEventData *) ed;
    cout << "Hypothesis: " << h->getHypothesis() << endl;
    if(h->getHypothesis() == "zero") {
		cout << "Switching to yes no dialogue" << endl;
		sr->updateJSGFPath("tests/confirm.gram");
		sr->applyUpdates();
    }
    else if(h->getHypothesis() == "yes") {
		cout << "Switching to digits dialogue" << endl;
		sr->updateJSGFPath("tests/digits.gram");
		sr->applyUpdates();
    }
    done->store(true);
}

void speechBegin(EventData * ed, atomic<bool> * done) {
    cout << "SPEECH STARTED" << endl;
    done->store(true);
}

void speechEnd(EventData * ed, atomic<bool> * done) {
    cout << "SPEECH ENDED" << endl;
    done->store(true);
}

int main() {
    cout << "Decoder Update Test" << endl;
	YAML::Node config;
	Buckey::applyDefaultSphinxConfig(config);

    syslog(LOG_DEBUG, "Attempting to start speech recognition.");
    sr = new SphinxDecoderManager(config, 2);
    sr->addOnHypothesis(printHypothesis);
    sr->addOnSpeechStart(speechBegin);
    sr->addOnSpeechEnd(speechEnd);
    sr->updateJSGFPath("tests/digits.gram");
    sr->updateSearchMode(SphinxHelper::SearchMode::JSGF);
    sr->updateLogPath("/home/tyler/Documents/sphinx.log");
    sr->applyUpdates();
    sr->startDeviceRecognition("plughw:CARD=Headset,DEV=0");

    char i;
    cin >> i;
    cout << "FINISHED" << endl;
    sr->stopRecognition();
    delete sr;
    cout << "DONE" << endl;
}
