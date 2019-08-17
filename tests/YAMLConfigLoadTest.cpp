#include <iostream>
#include <vector>
#include <string>
#include <syslog.h>

#include "yaml-cpp/yaml.h"

#define INPUT_FILE "tests/test-config.yaml"

using namespace std;

int main() {
	cout << "YAML Configuration Loading Test" << endl;
	cout << "Loading from " << INPUT_FILE << endl;
	YAML::Node config = YAML::LoadFile(INPUT_FILE);
	cout << "Max # of decoders: " << std::to_string(config["max-decoders"].as<int>()) << endl;
	cout << "Recognition device: " << config["sphinx-device"].as<std::string>() << endl;
	cout << "Sphinx log file: " << config["sphinx-log"].as<std::string>() << endl;

	// You always need to check to see if a key exists before loading it, or else you will crash.
	if(config["BLARG"]) {
		cout << "This config key doesn't exist: " << config["BLARG"].as<std::string>() << endl;
	}
}
