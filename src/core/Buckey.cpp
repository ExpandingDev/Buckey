#include "Buckey.h"
#include "MimicTTSService.h"
#include "SphinxService.h"
#include "SphinxMode.h"
#include "OutputEventData.h"
#include "poll.h"

#include <future>
#include <chrono>

Buckey * Buckey::instance = nullptr;
bool Buckey::instanceSet = false;
std::mutex Buckey::tempIDLock;
std::mutex Buckey::logLock;
FILE * Buckey::logFile;
unsigned long Buckey::nextTempID = 0;

Buckey::Buckey() : running(true), killed(false), inConversation(false), socketConnected(false)
{
	Buckey::logFile = fopen(LOG_FILE, "a");
	logInfo("Buckey being constructed.");
    configDir = cppfs::fs::open("config");
    assetsDir = cppfs::fs::open("assets");
    tempDir = cppfs::fs::open("tmp");
    nextTempID = 0;
}

Buckey::Buckey(cppfs::FileHandle confDir, cppfs::FileHandle assetDir, cppfs::FileHandle tmpDir) : running(true), killed(false), inConversation(false), socketConnected(false)
{
	Buckey::logFile = fopen(LOG_FILE, "a");
	logInfo("Buckey being constructed.");
    configDir = confDir;
    assetsDir = assetDir;
    tempDir = tmpDir;
    nextTempID = 0;
}

Buckey::~Buckey() {
	logDebug("Received kill request. Deconstructing.");
	killed.store(true);
	inputWatcher.join();
	//Stop listening on the unix socket
	socketManagementThread.join();
	close(socket2Handle);
	close(socketHandle);

	//Save enabled services
	cppfs::FileHandle servicesEnabledList = coreConfigDir.open("services.enabled");
	std::unique_ptr<std::ostream> o = servicesEnabledList.createOutputStream();
	std::vector<std::string> enabledServices;
	listEnabledServices(enabledServices);
	for(std::string s : enabledServices) {
		o->write((s+"\n").c_str(), s.length() + 1);
	}
	o->flush();

	//Save enabled modes
	cppfs::FileHandle modesEnabledList = coreConfigDir.open("modes.enabled");
	o = modesEnabledList.createOutputStream();
	std::vector<std::string> enabledModes;
	listEnabledModes(enabledModes);
	for(std::string s : enabledModes) {
		o->write((s+"\n").c_str(), s.length() + 1);
	}
	o->flush();

	//Send stop signal to all services
	for(unsigned int i = 0; i < services.size(); i++) {
		services[i]->stop();
	}

	//Delete all of the services
	for(unsigned int i = 0; i < services.size(); i++) {
		delete services[i];
	}

	//Delete all of the modes
	for(unsigned int i = 0; i < modes.size(); i++) {
		delete modes[i];
	}

	//Free all built in sound effects
	for(std::pair<SoundEffects, Mix_Chunk *> p : soundBank) {
		Mix_FreeChunk(p.second);
	}

	delete rootGrammar;

	Mix_Quit();
	SDL_CloseAudio();

	instanceSet = false;
	running = false;
	fclose(Buckey::logFile);
	syslog(LOG_INFO, "Buckey instance exited.");
	closelog();
}

void Buckey::logDebug(std::string message) {
	Buckey::logLock.lock();
	std::string m = "DEBUG: " + message + "\n";
	fwrite(m.c_str(), sizeof(char), m.size(), Buckey::logFile);
	Buckey::logLock.unlock();
}

void Buckey::logInfo(std::string message) {
	Buckey::logLock.lock();
	std::string m = "INFO: " + message + "\n";
	fwrite(m.c_str(), sizeof(char), m.size(), Buckey::logFile);
	Buckey::logLock.unlock();
}

void Buckey::logWarn(std::string message) {
	Buckey::logLock.lock();
	std::string m = "WARN: " + message + "\n";
	fwrite(m.c_str(), sizeof(char), m.size(), Buckey::logFile);
	Buckey::logLock.unlock();
}

void Buckey::logError(std::string message) {
	Buckey::logLock.lock();
	std::string m = "ERROR: " + message + "\n";
	fwrite(m.c_str(), sizeof(char), m.size(), Buckey::logFile);
	Buckey::logLock.unlock();
}

///This function is called after a new instance of Buckey is constructed via getInstance().
///Loads in all configuration and enables services and modes
void Buckey::init() {
	coreConfigDir = configDir.open("core");
	coreAssetsDir = assetsDir.open("core");

    coreConfig = coreConfigDir.open("buckey.yaml");

    //Set up the root grammar
    rootGrammar = new DynamicGrammar();
	modeList.reset(new AlternativeSet());
	rootExpansion.reset(new Sequence());
	((Sequence *) rootExpansion.get())->addChild(shared_ptr<Token>(new Token("buckey")));
	((Sequence *) rootExpansion.get())->addChild(modeList);
	rootGrammar->addRule(shared_ptr<Rule>(new Rule("root", true, rootExpansion)));

	registerAllServices();
	registerAllModes();

	readCoreConfig();

	initAudio();

	//setlogmask(LOG_DEBUG);
	enableMode("core");

	//Set up the unix socket
	//Start listening on the unix socket now that everything is ready
    makeServerSocket();
    socketManagementThread = std::thread(manageUnixSocket);

    //Start watching the inputQue
    inputWatcher = std::thread(watchInputQue, this);

    triggerEvents(ONFINISHINIT, new EventData());
}

///Called in Buckey::init(), loads SDL_mixer, sets up callbacks, loads built in sound effects into soundBank.
void Buckey::initAudio() {
	// start SDL with audio support
	if(SDL_Init(SDL_INIT_AUDIO)==-1) {
			printf("SDL_Init: %s\n", SDL_GetError());
			logError("Error initializing SDL_mixer");
			exit(1);
	}

	// open 44.1KHz, signed 16bit, system byte order,
	//      mono audio, using 1024 byte chunks
	if(Mix_OpenAudio(44100, AUDIO_S16SYS, 1, 1024)==-1) {
		printf("Mix_OpenAudio: %s\n", Mix_GetError());
		logError("Error opening SDL_mixer audio");
		exit(2);
	}

	Mix_Init(MIX_INIT_FLAC | MIX_INIT_MP3 | MIX_INIT_OGG);

    Mix_Chunk * m = Mix_LoadWAV(coreAssetsDir.open("attentionPing.wav").path().c_str());
    if(!m) {
		logError("Error opening attentionPing.wav");
    }
    soundBank.push_back(std::pair<SoundEffects,Mix_Chunk *>(SoundEffects::ATTENTION, m));

    m = Mix_LoadWAV(coreAssetsDir.open("errorPing.wav").path().c_str());
    if(!m) {
        logError("Error opening errorPing.wav");
    }
    soundBank.push_back(std::pair<SoundEffects,Mix_Chunk *>(SoundEffects::ERROR, m));

    m = Mix_LoadWAV(coreAssetsDir.open("okPing.wav").path().c_str());
    if(!m) {
        logError("Error opening okPing.wav");
    }
    soundBank.push_back(std::pair<SoundEffects,Mix_Chunk *>(SoundEffects::OK, m));

    m = Mix_LoadWAV(coreAssetsDir.open("readyPing.wav").path().c_str());
    if(!m) {
        logError("Error opening readyPing.wav");
    }
    soundBank.push_back(std::pair<SoundEffects,Mix_Chunk *>(SoundEffects::READY, m));

    m = Mix_LoadWAV(coreAssetsDir.open("alertPing.wav").path().c_str());
    if(!m) {
        logError("Error opening alertPing.wav");
    }
    soundBank.push_back(std::pair<SoundEffects,Mix_Chunk *>(SoundEffects::ALERT, m));

    Mix_ChannelFinished(Buckey::soundEffectDoneCallback);
}

bool Buckey::isMakingSound() {
	return soundsPlayingCount.load() != 0;
}

unsigned short Buckey::countSoundsPlaying() {
	return soundsPlayingCount.load();
}

bool Buckey::playSoundEffect(SoundEffects e, bool sync) {
	Mix_Chunk * c;
    for(std::pair<SoundEffects, Mix_Chunk *> p : soundBank) {
		if(p.first == e) {
			c = p.second;
		}
    }
    int h = Mix_PlayChannel(-1, c, false);
    if(h == -1) { // Error
		logError("Error playing sound effect!");
		return false;
    }
    else {
    	soundsPlayingCount.store(soundsPlayingCount+1);
    }

	if(sync) {
	    while(Mix_Playing(h)) {
			//Wait
	    }
	    soundsPlayingCount.store(soundsPlayingCount-1);
    }
    else {

    }

    return true;
}

///Internal callback for SDL_mixer to call when a channel finished playback.
///Used to keep track of the number of sounds playing, including sounds played asynchronously.
void Buckey::soundEffectDoneCallback(int channel) {
	Buckey * b = Buckey::getInstance();
	for(unsigned short i = 0; i < b->soundEffectChannels.size(); i++) {
		if(b->soundEffectChannels[i] == channel) {
			b->soundsPlayingCount.store(b->soundsPlayingCount-1);
			b->soundEffectChannels.erase(b->soundEffectChannels.begin() + i);
		}
	}
}

///Called in Buckey::init(), enables the services and modes enabled in services.enabled and modes.enabled config files
void Buckey::readCoreConfig() {
	cppfs::FileHandle servicesEnabledList = coreConfigDir.open("services.enabled");
	if(!servicesEnabledList.isFile()) {
		servicesEnabledList.writeFile("mimic\nsphinx\n");
	}
	std::istream * i = servicesEnabledList.createInputStream().release();
	char buff[60];
	while(i->getline(buff, 60) && !i->eof()) {
		enableService(buff);
	}
	delete i;

	cppfs::FileHandle modesEnabledList = coreConfigDir.open("modes.enabled");
	if(!modesEnabledList.isFile()) {
		//modesEnabledList.writeFile("mimic\nsphinx\n");
		///TODO: Fill these with defaults
	}
	i = modesEnabledList.createInputStream().release();
	//char buff[60];
	while(i->getline(buff, 60) && !i->eof()) {
		enableMode(buff);
	}
	delete i;

	coreConfigYAML = YAML::LoadFile(coreConfig.path());
}

///Registers all available services with Buckey, when adding in your own service, add it into this function if possible.
void Buckey::registerAllServices() {
 	MimicTTSService * mimicTTS = MimicTTSService::getInstance();
 	ttsService = mimicTTS;
 	registerService(mimicTTS);

 	SphinxService * sphinx = SphinxService::getInstance();
 	registerService(sphinx);
}

///Registers all available modes with Buckey, when adding in your own mode, add it into this function if possible.
void Buckey::registerAllModes() {
	coreMode = CoreMode::getInstance();
	registerMode(coreMode);

	registerMode(EchoMode::getInstance());

	registerMode(SphinxMode::getInstance());
}

///Creates the UNIX server socket
void Buckey::makeServerSocket() {
	///Many thanks to: http://beej.us/guide/bgipc/html/multi/unixsock.html
    socketHandle = socket(AF_UNIX, SOCK_STREAM, 0);
    if(socketHandle == -1) {
		syslog(LOG_ERR, "Error opening server unix socket!");
		std::cout << "Error opening server unix socket!" << std::endl;
	    exit(-1);
    }

    local.sun_family = AF_UNIX;
    std::string socketPath = coreConfigYAML["unix-socket-path"].as<std::string>();
    strcpy(local.sun_path, socketPath.c_str());
    unlink(local.sun_path);
    int len = strlen(local.sun_path) + sizeof(local.sun_family);
	int res = bind(socketHandle, (struct sockaddr *)&local, len);
	if(res == -1) {
		syslog(LOG_ERR, "Error binding server unix socket!");
		std::cout << "Error binding server unix socket!" << std::endl;
	    exit(-1);
	}

	if(listen(socketHandle, 1) == -1) {
		syslog(LOG_ERR, "Error while requesting to listen to server unix socket!");
		std::cout << "Error while requesting to listen to server unix socket!" << std::endl;
	    exit(-1);
	}
}

///Thread that listens for connections to the UNIX socket and reads input
void Buckey::manageUnixSocket() {
	int currentReadSize = 0;
	std::string s;
	char readBuffer[SOCKET_BUFFER_SIZE];
	unsigned int t = sizeof(remote);
	Buckey * b = getInstance();

	struct pollfd pollstruct;
	pollstruct.fd = b->socketHandle;
	pollstruct.events = POLLIN;


	while(!b->killed.load()) {

		if(poll(&pollstruct, 1, 500) > 0) {
			s = "";
			if ((b->socket2Handle = accept(b->socketHandle, (struct sockaddr *)&(b->remote), &t)) == -1) {
				if(errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
				else {
					syslog(LOG_ERR, "Error while accepting connection to unix socket!");
					logError("Error while accepting connection to unix socket!");
					std::cout << "Error while accepting connection to unix socket!" << std::endl;
					exit(1);
				}
			}
		}
		else {
			continue;
		}


        logInfo("Accepted connection to unix socket...");
        b->socketConnected.store(true);
        int numBytesRead = 0;
        bool doneReadingConnection = false;
        bool gotLine = false;
		memset(readBuffer, 0, SOCKET_BUFFER_SIZE);
        while (!doneReadingConnection && !b->killed.load()) {

			numBytesRead = recv(b->socket2Handle, readBuffer, SOCKET_BUFFER_SIZE-1, MSG_DONTWAIT); // Non blocking so that we can keep tabs on the status of buckey
			if(numBytesRead < 0) {
				if(errno == EWOULDBLOCK || errno == EAGAIN) {
                    if(!b->isRunning()) { // Check to see if buckey ended yet, close sockets if it has
						break;
                    }
                    continue;
				}
				else {
					doneReadingConnection = true;
					gotLine = false;
					//Probably not an error, probably a disconnect
					break;
				}
			}
			else if(numBytesRead == 0) {
				doneReadingConnection = true;
				gotLine = false;
				break;
			}
			else {
				for(unsigned short i = 0; i < numBytesRead; i++) {
					s += readBuffer[i];
					currentReadSize++;
					if(currentReadSize > MAX_COMMAND_SIZE) {
						b->reply("The command you are entering is too large!", ReplyType::ALERT);
						break;
					}

					if(readBuffer[i] == '\n') {
						gotLine = true;
						b->passInput(s);
						s = "";
						currentReadSize = 0;
					}
				}
				memset(readBuffer, 0, SOCKET_BUFFER_SIZE);
			}
        }

        if(b->killed.load()) {
			break;
        }

        if(!gotLine) {
			b->socketConnected.store(false);
        	close(b->socket2Handle);
		}
	}

	close(b->socketHandle);
}

/**	\brief Attempts to pass input to the correct Mode
  *
  * Buckey will attempt to match the supplied input to the correct Mode through the JSGF root grammar.
  * If no match is found, the command is not passed on, and the root grammar is outputted to the user.
  * \param [in] Command to pass to Buckey
  *
  */
void Buckey::passCommand(std::string command) {
	MatchResult m = rootGrammar->match(command);
	std::cout << command << std::endl;

	if(m.matches) {
		std::vector<std::string> tags = m.getMatchingTags();
		for(std::vector<std::string>::iterator i = tags.begin(); i != tags.end(); i++) {
			std::string s = *i;
			if(StringHelper::stringStartsWith(s, MODE_TAG_PREFIX)) {
				std::string modeName = s.substr(strlen(MODE_TAG_PREFIX), s.length() - strlen(MODE_TAG_PREFIX));
				std::cout << "Mode: " << modeName << std::endl;
				tags.erase(i);
				//tags[0] = modeName;
				for(Mode * m : modes) {
					if(m->getName() == modeName && m->getState() == ModeState::ENABLED) {
						m->input(command, tags);
						return;
					}
				}
			}
		}
	}
	else {
		reply(rootGrammar->getText(), ReplyType::CONSOLE);
		reply("Matches tags: " + StringHelper::concatenateStringVector(m.getMatchingTags(), ','), ReplyType::CONSOLE);
	}
}


/// \brief An internal loop that runs in the inputWatcher thread that checks the inputQue and passes input commands to Modes
void Buckey::watchInputQue(Buckey * b) {
	while(!b->killed.load()) {
		b->inputQueMutex.lock();

        if(!b->inputQue.empty()) {
			if(!b->isInConversation()) {
				std::string s = b->inputQue.front();
				b->inputQue.pop();
				b->inputQueMutex.unlock();
				b->passCommand(s);
			}
			else {
				b->inputQueMutex.unlock();
			}
        }
        else {
			b->inputQueMutex.unlock();
		}

	}
}

/**	\brief Sends a message/communicates with the user
  *
  * Method to send feedback to the user.
  * \param message [in] Message to pass onto the user.
  * \param ReplyType [in] Specifies the type of message that is being relayed. See ReplyType for more details.
  */
void Buckey::reply(std::string message, ReplyType t) {

	std::string out;
	if(t == ReplyType::CONVERSATION) {
		out = "CONV:" + message;
	}
	else if(t == ReplyType::CRITICAL) {
		logWarn("Received CRITICAL message: " + message);
		syslog(LOG_WARNING, "Received CRTICIAL message type: %s", message.c_str());
		out = "CRIT:" + message;
	}
	else if(t == ReplyType::STATUS) {
		out = "STAT:" + message;
	}
	else if(t == ReplyType::PROMPT) {
		out = "????:" + message;
	}
	else if(t == ReplyType::CONSOLE) {
		std::cout << message << std::endl;
		return;
	}

	std::cout << out << std::endl;
	out = out + "\n";

	if(socketConnected) {
		char output[256];
		strncpy(output, out.c_str(), 255);
		send(socket2Handle, output, sizeof(output), MSG_DONTWAIT); // Don't block in case the client disconnects, this could lead to the suddenly disconnecting client eating up all available connections.
	}

	triggerEvents(ONOUTPUT, new OutputEventData(message, t));
}

/**
  * \brief Attempts to lock the conversationMutex, does not return until the mutex is locked. Ensures that only one conversation is happening at a time.
  */
void Buckey::startConversation() {
	conversationMutex.lock();
	inConversation.store(true);
}

/// \brief Unlocks the conversationMutex. Call when finished with your conversation so that new conversations may start.
void Buckey::endConversation() {
	conversationMutex.unlock();
	inConversation.store(false);
}

/// \brief Returns true if Buckey is in a conversation.
bool Buckey::isInConversation() {
	return inConversation.load();
}


/// \brief Passes an input sting to the inputQue that will later be handled by the Mode in the Conversation or by the inputQueWatcher that will pass it to a command.
void Buckey::passInput(std::string input) {
	inputQueMutex.lock();
	inputQue.push(input);
	inputQueMutex.unlock();
}

/// \brief Prompts the user yes or no and will timeout after the specified amount of time if no input is received.
/// \param const std::string prompt - The prompt that will be displayed to the user.
/// \param int timeout - The timeout in second that the user had to reply by before the prompt times out.
PromptResult * Buckey::promptConfirmation(const std::string & prompt, int timeout) {
	reply(prompt, ReplyType::PROMPT);
	std::string result;
	triggerEvents(ONENTERPROMPT, new PromptEventData("confirm"));
	std::chrono::system_clock::time_point timeoutPassed = std::chrono::system_clock::now() + std::chrono::seconds(timeout);

	///TODO: Get sphinx to switch JSGF grammars and listen for yes/no/confirmation dialogue.

	bool timedOut = false;
	bool found = false;
    while(!killed.load() && !found) {
    	if(std::chrono::system_clock::now() > timeoutPassed) {
			timedOut = true;
			break;
    	}

		inputQueMutex.lock();
		if(!inputQue.empty()) {
			found = true;
			result=inputQue.front();
			inputQue.pop();
			inputQueMutex.unlock();
			break;
		}
		inputQueMutex.unlock();
    }

    triggerEvents(ONEXITPROMPT, new EventData());
    ///TODO: Have sphinx switch back to normal JSGF grammar

	if(timedOut) {
		return new PromptResult();
	}
	else {
		bool * confirm = new bool();
		if(result == "confirm") {
			*confirm = true;
		}
		else {
			*confirm = false;
		}
		return new PromptResult(confirm);
	}
}

void Buckey::addModeToRootGrammar(const Mode * m, DynamicGrammar * g) {
	std::shared_ptr<Rule> r = g->getRule("command");
	if(r == nullptr) {
		logWarn("Could not find <command> rule in mode grammar " + g->getName() + "! Not appending to root grammar.");
		return;
	}
	rootGrammarLock.lock();
	//Append the top level command rule
	if(rootGrammar->getRule(m->getName()) != nullptr) { // Mode is already added, update its rule expansion
		rootGrammar->getRule(m->getName())->setRuleExpansion(r->getRuleExpansion());
	}
	else { // Mode has not been appended yet
		std::shared_ptr<Expansion> e = r->getRuleExpansion();
		std::shared_ptr<Rule> newRule = std::shared_ptr<Rule>(new Rule(m->getName(), false, e));
		rootGrammar->addRule(newRule);

		modeList->addChild(std::shared_ptr<Tag>(new Tag(shared_ptr<RuleReference>(new RuleReference(m->getName())), MODE_TAG_PREFIX + m->getName())));
	}

	//Append the subsequent variable rules
    for(std::shared_ptr<Rule> v : g->getRules()) {
        if(v->getRuleName() != "command") { // We already appended the command rule
			if(rootGrammar->getRule(v->getRuleName()) != nullptr) { // Rule is already added, update its rule expansion
				rootGrammar->getRule(v->getRuleName())->setRuleExpansion(v->getRuleExpansion());
			}
			else { // Rule has not been appended yet
				rootGrammar->addRule(v);
			}
        }
    }
    rootGrammarLock.unlock();
}

void Buckey::removeModeFromRootGrammar(const Mode * m, DynamicGrammar * g) {
	std::shared_ptr<Rule> r = g->getRule("command");
	if(r == nullptr) {
		logWarn("Could not find <command> rule in mode grammar " + g->getName() + "! Not removing from root grammar.");
		return;
	}

	rootGrammarLock.lock();
	//Remove all rules found in the Dynamic Grammar
    for(std::shared_ptr<Rule> v : g->getRules()) {
        if(v->getRuleName() != "command") { // We already appended the command rule
			rootGrammar->removeRule(v->getRuleName());
        }
        else {
	        rootGrammar->removeRule(m->getName());
        }
    }

    for(unsigned int i = 0; i < modeList->childCount(); i++) {
		if(((Tag *) modeList->getChild(i).get())->getTags()[0] == MODE_TAG_PREFIX + m->getName()) {
            modeList->removeChild(i);
            break;
		}
    }
    rootGrammarLock.unlock();
}

///Enables the specified service if the service exists
void Buckey::enableService(const std::string & serviceName) {
	for(Service * s : services) {
		if(s->getName() == serviceName) {
			logInfo("Enabling service " + serviceName);
			s->enable();
			triggerEvents(ONSERVICEENABLE, new ServiceControlEventData(s));
		}
	}
}

///Stops the specified service if the service exists
void Buckey::stopService(const std::string & serviceName) {
	for(Service * s : services) {
		if(s->getName() == serviceName) {
			s->stop();
			triggerEvents(ONSERVICESTOP, new ServiceControlEventData(s));
		}
	}
}

///Reloads the specified service if the service exists
void Buckey::reloadService(const std::string & serviceName) {
	for(Service * s : services) {
		if(s->getName() == serviceName) {
			s->reload();
		}
	}
}

///Disables the specified service if the service exists
void Buckey::disableService(const std::string & serviceName) {
	for(Service * s : services) {
		if(s->getName() == serviceName) {
			s->stop();
			s->disable();
			triggerEvents(ONSERVICEDISABLE, new ServiceControlEventData(s));
		}
	}
}

///This method adds the service to Buckey's service list and changes the Service's state from SERVICE_NOT_REGISTED to DISABLED.
void Buckey::registerService(Service * service) {
	service->registered(); // Change the state from NOT_REGISTERED to DISABLED

	cppfs::FileHandle serviceConfigDir = configDir.open("service").open(service->getName());
	cppfs::FileHandle serviceAssetsDir = assetsDir.open("service").open(service->getName());

	if(!serviceConfigDir.isDirectory()) {
		logWarn("Could not find config directory for service " + service->getName() + "! Setting one up...");
		if(serviceConfigDir.createDirectory()) {
			service->setupConfig(serviceConfigDir); // The service's config directory does not exist, so this must be a first run, have the service set up its default config
		}
		else {
			logError("Failed to create configuration directory for service " + service->getName() + "! Check file permissions!");
		}
	}

	if(!serviceAssetsDir.isDirectory()) {
		logWarn("Could not find assets directory for service "+service->getName()+"! Setting one up...");
		if(serviceAssetsDir.createDirectory()) {
			service->setupAssets(serviceAssetsDir);
		}
		else {
			logError("Failed to create assets directory for service "+service->getName()+"! Check file permissions!");
		}
	}

	service->setConfigDir(serviceConfigDir);
	service->setAssetsDir(serviceAssetsDir);
	services.push_back(service);

	triggerEvents(ONSERVICEREGISTER, new ServiceControlEventData(service));
}

///Enables the specified mode.
/// \param name [in] name of the mode to be enabled.
void Buckey::enableMode(const std::string & name) {
	for(Mode * m : modes) {
		if(m->getName() == name) {
			if(m->getState() == ModeState::DISABLED || m->getState() == ModeState::NOT_LOADED) {
				logInfo("Enabling mode " + m->getName());
				m->enable();
				triggerEvents(ONMODEENABLE, new ModeControlEventData(m));
			}
		}
	}
}

///Disables the specified mode.
/// \param name [in] name of the mode to be disabled.
void Buckey::disableMode(const std::string & name) {
	for(Mode * m : modes) {
		if(m->getName() == name) {
			if(m->getState() == ModeState::ENABLED || m->getState() == ModeState::ERROR) {
				logInfo("Disabling mode " + m->getName());
				m->disable();
				triggerEvents(ONMODEDISABLE, new ModeControlEventData(m));
			}
		}
	}
}

void Buckey::registerMode(Mode * m) {
	if(getModeState(m->getName().c_str()) == ModeState::NOT_LOADED) {
		cppfs::FileHandle modeAssetsDir = assetsDir.open("mode").open(m->getName());
		if(!modeAssetsDir.isDirectory()) {
			logWarn("Could not find assets directory for mode "+m->getName()+", setting one up...");
			if(modeAssetsDir.createDirectory()) {
				m->setupAssetsDir(modeAssetsDir);
			}
			else {
				logError("Unable to create assets directory for mode "+m->getName()+"!");
			}
		}

		cppfs::FileHandle modeConfigDir = configDir.open("mode").open(m->getName());
		if(!modeConfigDir.isDirectory()) {
			logWarn("Could not find config directory for mode "+m->getName()+", setting one up...");
			if(modeConfigDir.createDirectory()) {
				m->setupConfigDir(modeConfigDir);
			}
			else {
				logError("Unable to create config directory for mode "+m->getName()+"!");
			}
		}

		m->setAssetsDir(modeAssetsDir);
		m->setConfigDir(modeConfigDir);

		modes.push_back(m);

		triggerEvents(ONMODEREGISTER, new ModeControlEventData(m));
	}
}

///Reloads the specified mode.
/// \param name [in] name of the mode to be reloaded.
void Buckey::reloadMode(const std::string & name) {
	disableMode(name);
	enableMode(name);
}

///Returns the ModeState that the specified Mode is in. If the Mode cannot be found, returns ModeState::NOT_LOADED
/// \param name [in] name of the mode to be polled.
const ModeState Buckey::getModeState(const std::string & name) {
	for(Mode * m : modes) {
		if(m->getName() == name) {
			return m->getState();
		}
	}
	return ModeState::NOT_LOADED;
}

///Pushes into output vector the names of the services that are enabled.
/// \param output [out] vector of strings of all the names of the services that are enabled
const void Buckey::listEnabledServices(std::vector<std::string> & output) {
	for(Service * s : services) {
		ServiceState state = s->getState();
		if(state == ServiceState::STARTING || state == ServiceState::RUNNING || state == ServiceState::STOPPED) {
			output.push_back(s->getName());
		}
	}
}

///Pushes into output vector the names of the services registered with Buckey
/// \param output [out] vector of strings of all the names of the services registered with Buckey
const void Buckey::listServices(std::vector<std::string> & output) {
	for(Service * s : services) {
		output.push_back(s->getName());
	}
}

///Pushes into output vector the names of the services in the selected state
/// \param output [out] vector of strings of all the names of the services in the target ServiceState
/// \param state [in] ServiceState that is being requested
const void Buckey::listServicesInState(std::vector<std::string> & output, ServiceState state) {
	for(Service * s : services) {
		if(s->getState() == state) {
			output.push_back(s->getName());
		}
	}
}

///Pushes into output vector the names of the modes that are enabled.
/// \param output [out] vector of strings of all the names of the modes that are enabled
const void Buckey::listEnabledModes(std::vector<std::string> & output) {
	for(Mode * m : modes) {
		ModeState state = m->getState();
		if(state == ModeState::ENABLED) {
			output.push_back(m->getName());
		}
	}
}

///Pushes into output vector the names of the modes registered with Buckey
/// \param output [out] vector of strings of all the names of the modes registered with Buckey
const void Buckey::listModes(std::vector<std::string> & output) {
	for(Mode * m : modes) {
		output.push_back(m->getName());
	}
}

///Pushes into output vector the names of the modes in the selected state
/// \param output [out] vector of strings of all the names of the modes in the target ModeState
/// \param ms [in] ModeState that is being requested
const void Buckey::listModesInState(std::vector<std::string> & output, const ModeState ms) {
	for(Mode * m : modes) {
		if(m->getState() == ms) {
			output.push_back(m->getName());
		}
	}
}

///Returns a cppfs::FileHandle to a reserved temporary file in the tmp dir with the specified extension
/// \param extension [in] The desired extension of the temporary file.
/// \return cppfs::FileHandle to the resulting temporary file in the tmp dir.
cppfs::FileHandle Buckey::getTempFile(const std::string & extension) {
	tempIDLock.lock();
	unsigned long id = nextTempID;
	nextTempID++;
	tempIDLock.unlock();
    return tempDir.open(std::to_string(id) + extension);
}

///Returns the specified service state if the service exists. If it does not exist, returns SERVICE_NOT_REGISTERED.
/// \param name [in] name of the service
/// \return ServiceState of the specified service. SERVICE_NOT_REGISTERED if it could not be found.
const ServiceState Buckey::getServiceState(const std::string & serviceName) {
	for(Service * s : services) {
		if(s->getName() == serviceName) {
			return s->getState();
		}
	}
	return ServiceState::SERVICE_NOT_REGISTERED;
}

///Will return nullptr if the specified service name is not found!
/// \param name [in] name of the service that is wanted
/// \return Pointer to the requested service. nullptr if the service is not found.
const Service * Buckey::getService(const std::string & serviceName) {
	for(Service * s : services) {
		if(s->getName() == serviceName) {
			return s;
		}
	}
	return nullptr;
}

const bool Buckey::isKilled() {
	return killed.load();
}

const bool Buckey::isRunning() {
	return running.load();
}

///TODO: Make this const? Maybe not?
DynamicGrammar * Buckey::getRootGrammar() {
	return rootGrammar;
}

///Changes running to false and should trigger the shutdown of Buckey
void Buckey::requestStop() {
	killed.store(true);
}

///Reloads all services and then reloads all modes
void Buckey::reload() {
	logInfo("Asked to reload, reloading...");
	std::vector<std::string> out;
	listEnabledServices(out);
	for(std::string s : out) {
		reloadService(s);
	}
	out.clear();

	listEnabledModes(out);
	for(std::string s : out) {
		reloadMode(s);
	}
}
