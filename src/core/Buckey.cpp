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
	requestStop();
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

	//Stop all started Modes
	for(unsigned int i = 0; i < modes.size(); i++) {
		if(modes[i].second->getState() == ModeState::STARTED) {
			modes[i].second->stop();
		}
	}

	//Stop all running Services
	for(unsigned int i = 0; i < services.size(); i++) {
		if(services[i].second->getState() == ServiceState::RUNNING) {
			services[i].second->stop();
		}
	}

	//Sleep for a second to let loose threads close up
	std::this_thread::sleep_for (std::chrono::seconds(1));

	//Delete all of the modes
	for(unsigned int i = 0; i < modes.size(); i++) {
		delete modes[i].second;
	}

	//Delete all of the services
	for(unsigned int i = 0; i < services.size(); i++) {
		delete services[i].second;
	}

	//Free all built in sound effects
	for(std::pair<SoundEffects, Mix_Chunk *> p : soundBank) {
		Mix_FreeChunk(p.second);
	}

	delete rootGrammar;

	Mix_Quit();
	SDL_CloseAudio();

	instanceSet = false;

	fclose(Buckey::logFile); // Close our own log file
	syslog(LOG_INFO, "Buckey instance exited.");
	closelog(); // Close the syslog file
	running.store(false);
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
	std::cerr << m;
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

	services.clear();
	modes.clear();

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
///TODO: Make adding in your own services easier
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
						if(s.size() > 0) {
							if(s[0] == ':') {
								s[0] = ' ';
								b->passCommand(s);
							}
							else {
								b->passInput(s);
							}
						}
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

	if(m.matches) { //Process the user input if it matches a command
		std::vector<std::string> tags = m.getMatchingTags(); //Get the tags that it matches
		for(std::vector<std::string>::iterator i = tags.begin(); i != tags.end(); i++) { // Iterate until we find the tag that tells use which mode this input matches (MODE_TAG_PREFIX)
			std::string s = *i;
			if(StringHelper::stringStartsWith(s, MODE_TAG_PREFIX)) { // Found the tag telling us which mode it matches, extract the mode and pass the remaining tags and the input to the correct mode
				std::string modeName = s.substr(strlen(MODE_TAG_PREFIX), s.length() - strlen(MODE_TAG_PREFIX));
				std::cout << "Mode: " << modeName << std::endl;
				tags.erase(i);
				for(modeListEntry m : modes) {
					if(m.second->getName() == modeName && m.second->getState() == ModeState::STARTED) {
						m.second->input(command, tags);
						return;
					}
				}
			}
		}
	}
	else { // Output the root grammar for debugging purposes if the user input does not match
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

	Buckey::logInfo(out);

	triggerEvents(ONOUTPUT, new OutputEventData(message, t));
}

/**
  * \brief Attempts to lock the conversationMutex, does not return until the mutex is locked. Ensures that only one conversation is happening at a time.
  */
void Buckey::startConversation() {
	conversationMutex.lock();
	inConversation.store(true);
	triggerEvents(ONCONVERSATIONSTART, new EventData());
}

/// \brief Unlocks the conversationMutex. Call when finished with your conversation so that new conversations may start.
void Buckey::endConversation() {
	conversationMutex.unlock();
	inConversation.store(false);
	triggerEvents(ONCONVERSATIONEND, new EventData());
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
			result = inputQue.front();
			Buckey::reply("Got result: " + result, ReplyType::CONSOLE);
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

///TODO: Make registering Modes and Services error out when registering two services or modes with the same name
///Starts and enables on startup the specified service if the service exists
void Buckey::enableService(const std::string & serviceName) {
	for(unsigned int i = 0; i < services.size(); i++) {
		serviceListEntry s = services[i];
		if(s.second->getName() == serviceName) {
			logInfo("Enabling service " + serviceName);
			services[i].first = true; // Enable the service on startup
			s.second->start(); // Start the service
			triggerEvents(ONSERVICEENABLE, new ServiceControlEventData(s.second));
			return;
		}
	}
}

///Starts the specified service if the service exists
void Buckey::startService(const std::string & serviceName) {
	for(serviceListEntry s : services) {
		if(s.second->getName() == serviceName) {
			logInfo("Starting service " + serviceName);
			s.second->start(); // Start the service
			triggerEvents(ONSERVICESTART, new ServiceControlEventData(s.second));
			return;
		}
	}
}

///Reloads (stops and starts) the specified service if the service exists
void Buckey::reloadService(const std::string & serviceName) {
	for(serviceListEntry s  : services) {
		if(s.second->getName() == serviceName) {
			s.second->reload();
			return;
		}
	}
}

///Stops and disables from startup the specified service if the service exists
void Buckey::disableService(const std::string & serviceName) {
	for(unsigned int i = 0; i < services.size(); i++) {
		serviceListEntry s = services[i];
		if(s.second->getName() == serviceName) {
			services[i].first = false; // Disable the service
			s.second->stop(); // Stop the service
			triggerEvents(ONSERVICEDISABLE, new ServiceControlEventData(s.second));
			return;
		}
	}
}

///Stops the specified service if the service exists
void Buckey::stopService(const std::string & serviceName) {
	for(serviceListEntry s : services) {
		if(s.second->getName() == serviceName) {
			s.second->stop(); // Stop the service
			triggerEvents(ONSERVICESTOP, new ServiceControlEventData(s.second));
			return;
		}
	}
}

///This method adds the service to Buckey's service list and changes the Service's state from SERVICE_NOT_REGISTED to DISABLED.
void Buckey::registerService(Service * service) {
	if(getServiceState(service->getName()) != ServiceState::SERVICE_NOT_REGISTERED) {
		///TODO: Complain to an error log about registering a service with the same name as a service that already exists.
		return;
	}
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
	services.push_back(serviceListEntry(false,service));

	triggerEvents(ONSERVICEREGISTER, new ServiceControlEventData(service));
}

///Starts the specified mode.
void Buckey::startMode(const std::string & name) {
	for(modeListEntry m : modes) {
		if(m.second->getName() == name) {
			if(m.second->getState() == ModeState::STOPPED || m.second->getState() == ModeState::NOT_LOADED) {
				logInfo("Starting mode " + name);
				m.second->start();
				triggerEvents(ONMODESTART, new ModeControlEventData(m.second));
			}
			return;
		}
	}
}

///Stops the specified mode.
void Buckey::stopMode(const std::string & name) {
	for(modeListEntry m : modes) {
		if(m.second->getName() == name) {
			if(m.second->getState() == ModeState::STARTED || m.second->getState() == ModeState::ERROR) {
				logInfo("Stopping mode " + name);
				m.second->stop();
				triggerEvents(ONMODESTOP, new ModeControlEventData(m.second));
			}
			return;
		}
	}
}

///Starts the specified mode and enables it on startup.
/// \param name [in] name of the mode to be enabled.
void Buckey::enableMode(const std::string & name) {
	for(unsigned int i = 0; i < modes.size(); i++) {
		modeListEntry m = modes[i];
		if(m.second->getName() == name) {
			modes[i].first = true; // Enable the mode on startup
			if(m.second->getState() == ModeState::STOPPED || m.second->getState() == ModeState::NOT_LOADED) {
				logInfo("Enabling mode " + name);
				m.second->start();
				triggerEvents(ONMODEENABLE, new ModeControlEventData(m.second));
			}
			return;
		}
	}
}

///Stops the specified Mode and disables it from startup if it exists.
/// \param name [in] name of the mode to be disabled.
void Buckey::disableMode(const std::string & name) {
	for(unsigned int i = 0; i < modes.size(); i++) {
		modeListEntry m = modes[i];
		if(m.second->getName() == name) {
			modes[i].first = false; // Disable the mode.
			if(m.second->getState() == ModeState::STARTED || m.second->getState() == ModeState::ERROR) {
				logInfo("Disabling mode " + m.second->getName());
				m.second->stop();
				triggerEvents(ONMODEDISABLE, new ModeControlEventData(m.second));
			}
			return;
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

		modes.push_back(modeListEntry(false, m));

		triggerEvents(ONMODEREGISTER, new ModeControlEventData(m));
	}
}

///Reloads the specified mode.
/// \param name [in] name of the mode to be reloaded.
void Buckey::reloadMode(const std::string & name) {
	stopMode(name);
	startMode(name);
}

///Returns the ModeState that the specified Mode is in. If the Mode cannot be found, returns ModeState::NOT_LOADED
/// \param name [in] name of the mode to be polled.
const ModeState Buckey::getModeState(const std::string & name) {
	for(modeListEntry m: modes) {
		if(m.second->getName() == name) {
			return m.second->getState();
		}
	}
	return ModeState::NOT_LOADED;
}

///Pushes into output vector the names of the services that are enabled on startup.
/// \param output [out] vector of strings of all the names of the services that are enabled on startup.
const void Buckey::listEnabledServices(std::vector<std::string> & output) {
	servicesList l = services;
	for(serviceListEntry s : services) {
		if(s.first == true) {
			output.push_back(s.second->getName());
		}
	}
}

///Pushes into output vector the names of the services that are disabled on startup.
/// \param output [out] vector of strings of all the names of the services that are disabled on startup.
const void Buckey::listDisabledServices(std::vector<std::string> & output) {
	servicesList l = services;
	for(serviceListEntry s : services) {
		if(s.first == false) {
			output.push_back(s.second->getName());
		}
	}
}

///Pushes into output vector the names of the services registered with Buckey
/// \param output [out] vector of strings of all the names of the services registered with Buckey
const void Buckey::listServices(std::vector<std::string> & output) {
	for(serviceListEntry s : services) {
		output.push_back(s.second->getName());
	}
}

///Pushes into output vector the names of the services in the selected state
/// \param output [out] vector of strings of all the names of the services in the target ServiceState
/// \param state [in] ServiceState that is being requested
const void Buckey::listServicesInState(std::vector<std::string> & output, ServiceState state) {
	for(serviceListEntry s : services) {
		if(s.second->getState() == state) {
			output.push_back(s.second->getName());
		}
	}
}

///Pushes into output vector the names of the modes that are enabled.
/// \param output [out] vector of strings of all the names of the modes that are enabled
const void Buckey::listEnabledModes(std::vector<std::string> & output) {
	for(modeListEntry m : modes) {
		if(m.first) {
			output.push_back(m.second->getName());
		}
	}
}

///Pushes into output vector the names of the modes that are disabled on startup.
/// \param output [out] vector of strings of all the names of the modes that are disabled
const void Buckey::listDisabledModes(std::vector<std::string> & output) {
	for(modeListEntry m : modes) {
		if(m.first == false) {
			output.push_back(m.second->getName());
		}
	}
}

///Pushes into output vector the names of the modes registered with Buckey
/// \param output [out] vector of strings of all the names of the modes registered with Buckey
const void Buckey::listModes(std::vector<std::string> & output) {
	for(modeListEntry m : modes) {
		output.push_back(m.second->getName());
	}
}

///Pushes into output vector the names of the modes in the selected state
/// \param output [out] vector of strings of all the names of the modes in the target ModeState
/// \param ms [in] ModeState that is being requested
const void Buckey::listModesInState(std::vector<std::string> & output, const ModeState ms) {
	for(modeListEntry m : modes) {
		if(m.second->getState() == ms) {
			output.push_back(m.second->getName());
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
	for(serviceListEntry s : services) {
		if(s.second->getName() == serviceName) {
			return s.second->getState();
		}
	}
	return ServiceState::SERVICE_NOT_REGISTERED;
}

///Will return nullptr if the specified service name is not found!
/// \param name [in] name of the service that is wanted
/// \return Pointer to the requested service. nullptr if the service is not found.
const Service * Buckey::getService(const std::string & serviceName) {
	for(serviceListEntry s : services) {
		if(s.second->getName() == serviceName) {
			return s.second;
		}
	}
	return nullptr;
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

///Returns true if a call to requestStop has been made. Child threads should exit all loops and Services should begin joining their child threads.
const bool Buckey::isKilled() {
	return killed.load();
}

///Reloads all services and then reloads all modes
void Buckey::reload() {
	logInfo("Asked to reload, reloading all running services and modes...");
	std::vector<std::string> servicesRunning;
	std::vector<std::string> modesRunning;
	listModesInState(modesRunning, ModeState::STARTED);
	listServicesInState(servicesRunning, ServiceState::RUNNING);

	for(std::string s : modesRunning) {
		stopMode(s);
	}

	for(std::string s : servicesRunning) {
		stopService(s);
	}

	for(std::string s : servicesRunning) {
		startService(s);
	}

	for(std::string s : modesRunning) {
		startMode(s);
	}
}
