#include <iostream>
#include <string>
#include <TTSService.h>
#include <MimicTTSService.h>

using namespace std;

int main() {
	cout << "Mimic TTS Test" << endl << "Initializing..." << endl;
	TTSService * tts = new MimicTTSService();
	cppfs::fs::open("mimic-test-assets").createDirectory();
	cppfs::fs::open("mimic-test-config").createDirectory();
	tts->setupConfig(cppfs::fs::open("mimic-test-config"));
	tts->setupAssets(cppfs::fs::open("mimic-test-assets"));
	tts->setAssetsDir(cppfs::fs::open("mimic-test-assets"));
	tts->setConfigDir(cppfs::fs::open("mimic-test-config"));

	cout << tts->getStatusMessage() << endl;
	tts->registered();
	cout << tts->getStatusMessage() << endl;
	tts->enable();
	cout << tts->getStatusMessage() << endl;

	cout << "Speaking..." << endl;
	((MimicTTSService *) (tts))->prepareSpeech("hi there");
	tts->speak("My name is Buckey");
	tts->asyncSpeak("hello there");
	tts->speak("hi there");
	tts->disable();
	cout << tts->getStatusMessage() << endl;

	cout << tts->handlerCount() << endl;

	//cout << "Error code: " << std::to_string(r) << endl;
	delete tts;
	cout << "DONE" << endl;
}
