#include <iostream>
#include <fstream>

#include "syslog.h"

///TODO: Find Windows replacement method for Daemonizing and unix sockets
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include "Buckey.h"
#include "Service.h"
#include "MimicTTSService.h"

#include "cppfs/FileHandle.h"
#include "cppfs/fs.h"

char RUNNING_DIR[200] = "~/Buckey";

//Determine the correct path separator
#ifdef _WIN32
#define PATH_SEPARATOR "\\"
#endif // _WIN32
#ifdef __linux__
#define PATH_SEPARATOR "/"
#endif // __linux__

#define LOCK_FILE "buckey.lock"
#define BUCKEY_VERSION "0.0.1"

Buckey * buckey;
YAML::Node coreConfig;

pid_t PID;

cppfs::FileHandle configDir, coreConfigDir, tempDir, assetsDir, coreAssetsDir, coreConfigFile;

void makeBuckey();
void doShutdown();
void signalHandler(int);
void daemonize();
bool setupDirectories();
void registerSignalHandles();
void badPipeHandler(int );

///This determines the correct directory that all Buckey instances will run in.
void resolveRunningDirectory() {
	strcpy(RUNNING_DIR, getenv("HOME"));
	strcat(RUNNING_DIR, PATH_SEPARATOR);
	strcat(RUNNING_DIR, "Buckey");
}

void registerSignalHandles() {
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,signalHandler); /* since we haven't daemonized yet, process the TTY signals */
	signal(SIGQUIT,signalHandler);
	signal(SIGHUP,signalHandler);
	signal(SIGTERM,signalHandler);
	signal(SIGPIPE,badPipeHandler);
}

void badPipeHandler(int signal) {
	cerr << "Received SIGPIPE (Bad Pipe), probably due to using the -c command, just ignore this." << std::endl;
	syslog(LOG_WARNING, "Received SIGPIPE (Bad Pipe), probably due to using the -c command, just ignore this.");
}

void doShutdown() {
	delete buckey;
	//Delete tmp dir before shutting down
	tempDir.removeDirectoryRec();
	syslog(LOG_DEBUG, "Cleaned up/deleted tmp directory...");
}

void signalHandler(int signal) {
	switch (signal){
		case SIGHUP:
			syslog(LOG_INFO,"SIGHUP Signal Received...");
			buckey->reload();
			break;
		case SIGQUIT:
		case SIGTSTP:
		case SIGTERM:
			syslog(LOG_INFO,"Terminate Signal Received...");
			buckey->requestStop();
			//exit(0);
			break;
		default:
			syslog(LOG_INFO, "Received unknown SIGNAL.");
			break;
	}
}

///A good portion of this code was borrowed from the example code provided at: http://www.enderunix.org/docs/eng/daemon.php
///Which was coded by: Levent Karakas <levent at mektup dot at> May 2001
///Many thank Levent!
void daemonize()
{
	int i,lfp;
	char str[10];
	syslog(LOG_DEBUG, "Daemonizing Buckey...");

	if(getppid() == 1) {
		return; /* already a daemon */
	}

	i=fork();
	if (i<0) {
		exit(1); /* fork error */
	}

	if (i>0) {
		exit(0); /* parent exits */
	}
	/* child (daemon) continues */

	setsid(); /* obtain a new process group */
	for (i=getdtablesize(); i >= 0; --i) {
		close(i); /* close all descriptors */
	}

	/* handle standard I/O */
	i=open("/dev/null",O_RDWR);
	dup(i);
	dup(i);

	umask(027); /* set newly created file permissions */

	lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);

	if (lfp<0) {
		exit(1); /* can not open */
	}

	if (lockf(lfp,F_TLOCK,0)<0) {
		syslog(LOG_INFO, "Unable to start Buckey! Lock file is already locked! Make sure that there isn't another Buckey process running!");
		exit(0); /* can not lock */
	}

	/* first instance continues */

	PID = getpid();
	sprintf(str,"%d\n",getpid());
	write(lfp,str,strlen(str)); /* record pid to lockfile */

	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP,signalHandler); /* catch hangup signal */
	signal(SIGTERM,signalHandler); /* catch kill signal */

	makeBuckey();

	std::string s;
	struct pollfd pollstruct;
	pollstruct.fd = 0; //stdin file descriptor
	pollstruct.events = POLLIN;

	while(buckey->isRunning()) {
		s = "";

		if(poll(&pollstruct, 1, 500) > 0) {
			getline(cin, s);
		}
		else {
			continue;
		}

		if(buckey->isRunning()) {
			if(s != "") {
				buckey->passCommand(s);
			}
		}
		else {
			std::cout << "Buckey has exited! Command not sent." << std::endl;
			break;
		}
	}

	delete buckey;
	exit(0);
}

void makeBuckey() {
	///The configuration, assets, and temp directories are all passed onto Buckey to use.
	///Buckey then handles loading configuration and enabling services
	buckey = Buckey::getInstance(configDir, assetsDir, tempDir);
}

bool setupDirectories() {
	//Open up or create config directory file structure
	bool firstRun = false;

    configDir = cppfs::fs::open("config");
	if(!configDir.isDirectory()) {
		firstRun = true;

		syslog(LOG_WARNING, "Could not find config directory, creating one!");
   		std::cout << "Could not find config directory, creating one!" << std::endl;

   		configDir.createDirectory();
   		configDir.open("service").createDirectory();
   		configDir.open("mode").createDirectory();
	}

	coreConfigDir = cppfs::fs::open("config/core");
	if(!coreConfigDir.isDirectory()) {
		syslog(LOG_WARNING, "Could not find config/core directory, creating one!");
		std::cout << "Could not find config/core directory, creating one!" << std::endl;
		if(!coreConfigDir.createDirectory()) {
			///ERROR: 	Could not create config/core directory!
			exit(-1);
		}
	}

	coreConfigFile = coreConfigDir.open("buckey.yaml");
	if(!coreConfigFile.exists()) {
		YAML::Emitter e;
		coreConfig["unix-socket-path"] = "buckey.socket";
		e << coreConfig;
		coreConfigFile.writeFile(e.c_str());
	}

	//Open up or create assets directory file structure
	assetsDir = cppfs::fs::open("assets");
	if(!assetsDir.isDirectory()) {

		syslog(LOG_WARNING, "Could not find assets directory, creating one!");

   		std::cout << "Could not find assets directory, creating one!" << std::endl;

   		assetsDir.createDirectory();
   		assetsDir.open("service").createDirectory();
   		assetsDir.open("mode").createDirectory();
	}

	coreAssetsDir = cppfs::fs::open("assets/core");
	if(!coreAssetsDir.isDirectory()) {
		syslog(LOG_WARNING, "Could not find assets/core directory, creating one!");
		std::cout << "Could not find assets/core directory, creating one!" << std::endl;
		if(!coreAssetsDir.createDirectory()) {
			///ERROR: Could not create assets/core directory!
			exit(-1);
		}
	}

	tempDir = cppfs::fs::open("tmp");
	if(tempDir.isDirectory()) { //Delete the old tmp directory if one exists already
		tempDir.removeDirectoryRec(); // Recursively delete old tmp dir

		tempDir = cppfs::fs::open("tmp"); //Reset the file handle, and check to make sure we actually deleted the old dir
		if(tempDir.isDirectory()) {
			syslog(LOG_ERR, "Could not remove old tmp directory! This bug should never happen! Exiting!");
			std::cout << "Could not remove old tmp directory! This bug should never happen! Exiting!" << std::endl;
			exit(-1);
		}
	}

	if(tempDir.createDirectory()) { // Create a new empty tmp directory
		syslog(LOG_DEBUG, "Created tmp directory...");
	}
	else {
		syslog(LOG_ERR, "Could not create new tmp directory! Exiting!");
		std::cout << "Could not create new tmp directory! Exiting!" << std::endl;
		exit(-1);
	}

	coreConfig = YAML::LoadFile(coreConfigFile.path());

	return firstRun;
}

int main(int argc, char *argv[])
{
	//First, process command line args
	bool makeDaemon = false;
	bool showHelp = false;
	bool showVersion = false;
	bool showStatus = false;
	bool executeCommand = false;
	bool executeInput = false;
	char * command;

    int c;
    opterr = 0;
	while ((c = getopt (argc, argv, "dhvsc:")) != -1) {
		switch (c) {
			case 'd':
				makeDaemon = true;
				break;
			case 'h':
				showHelp = true;
				break;
			case 'v':
				showVersion = true;
				break;
			case 's':
				showStatus = true;
				break;
			case 'c':
				executeCommand = true;
				command = optarg;
				break;
			case 'i':
				executeInput = true;
				command = optarg;
				break;
			case '?':
				if (isprint (optopt)) {
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				}
				else {
					fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
				return 1;
			default:
				exit(-1);
		}
	}

	if(showHelp) {
		std::cout << "Buckey" << std::endl << std::endl;
		std::cout << "\tBuckey is a privacy oriented, configurable, and open source personal assistant." << std::endl;
		std::cout << "Buckey provides input and output methods in the form of Services that run in the background." << std::endl;
		std::cout << "Buckey also provides simple command interfaces that can execute programs and scripts through Modes." << std::endl;
		std::cout << std::endl << "Usage Instructions:" << std::endl;
        std::cout << "\tbuckey [-c COMMAND | -i INPUT | -s | -h | -v | -d]" << std::endl << std::endl;
        std::cout << "Options:" << std::endl << "\t-c COMMAND\tPasses the specified command to the currently running Buckey daemon." << std::endl;
        std::cout << "\t-h\t\tShow this usage text." << std::endl;
        std::cout << "\t-s\t\tQuery the currently running Buckey daemon about its status." << std::endl;
	 	std::cout << "\t-c\t\tPass a command to the currently running Buckey instance." << std::endl;
	 	std::cout << "\t-i\t\tPass input to the currently running Buckey instance." << std::endl;
        std::cout << "\t-d\t\tStart a new Buckey daemon if one is not running already."<< std::endl;
        std::cout << "\t-v\t\tDisplay the current version of this program." << std::endl << std::endl;
        std::cout << "If no options are supplied, a new Buckey instance will be made unless another Buckey instance is already running." << std::endl;
        std::cout << "If another Buckey instance is running, this program will supply a command line interface to communicate with the running Buckey instance." << std::endl;
		std::cout << "Daemonization will fail if another Buckey instance is running." << std::endl;
		return 0;
	}

	if(showVersion) {
		std::cout << "Buckey version " << VERSION << std::endl;
		return 0;
	}

	resolveRunningDirectory();

	cppfs::FileHandle runningDirectory = cppfs::fs::open(RUNNING_DIR);
	if(!runningDirectory.isDirectory()) {
		if(!runningDirectory.createDirectory()) {
			std::cerr << "Failed to create running directory " << RUNNING_DIR << std::endl;
			exit(-1);
		}
		else {
			std::cout << "Created running directory " << RUNNING_DIR << std::endl;
		}
	}

	int res = chdir(RUNNING_DIR); /* change running directory */
	if(res < 0) {
		std::cerr << "Failed to change to running directory: " << RUNNING_DIR << std::endl;
		switch(errno){
			case EACCES:
				std::cerr << "Permision denied to change to that director" << std::endl;
				break;
			case ELOOP:
				std::cerr << "A symbolic link loop exists when resolving the target directory" << std::endl;
				break;
			case ENOTDIR:
				std::cerr << "The target directory is not a directory." << std::endl;
				break;
			case ENOENT:
				std::cerr << "No such directory" << std::endl;
				break;
			case ENAMETOOLONG:
				std::cerr << "Target directory path is too long" << std::endl;
				break;
			default:
				std::cerr << "Unknown error case!" << std::endl;
				break;
		}
		return res;
	}

	if(showStatus) {
		///TODO: Get Buckey's current status
		return 0;
	}

	//Open up logging
    openlog("buckey", LOG_CONS | LOG_NDELAY | LOG_PID, LOG_USER);
    //setlogmask(LOG_DEBUG);
    syslog(LOG_DEBUG, "Buckey Main Function Entered");

    std::cout << "Hello world! Buckey is starting up..." << std::endl;

	registerSignalHandles();

	int lfp = open(LOCK_FILE,O_RDWR|O_CREAT,0640);

	//Attempt to open up the lock file
	if (lfp<0) {
		std::cout << "Error opening lock file!" << std::endl;
		syslog(LOG_ERR, "Error opening lock file!");
		exit(1); /* can not open */
	}

	bool firstRun = setupDirectories();

	///TODO: Figure out what to do on first run
	if(!firstRun) {

	}

	//User requested to only send a command. Connect to the UNIX socket and send it.
	if(executeCommand || executeInput) {
		//Open the unix socket
		int socketHandle;
		struct sockaddr_un remote;

		socketHandle = socket(AF_UNIX, SOCK_STREAM, 0);
		if(socketHandle == -1) {
			syslog(LOG_ERR, "Error opening client unix socket!");
			std::cout << "Error opening client unix socket!" << std::endl;
			exit(-1);
		}

		remote.sun_family = AF_UNIX;
		std::string socketPath = coreConfig["unix-socket-path"].as<std::string>();
		strcpy(remote.sun_path, socketPath.c_str());

		int len = strlen(remote.sun_path) + sizeof(remote.sun_family);

		int res = connect(socketHandle, (struct sockaddr *)&remote, len);
		if(res == -1) {
			syslog(LOG_ERR, "Error connecting to server unix socket!");
			std::cout << "Error connecting to server unix socket! " << coreConfig["unix-socket-path"].as<std::string>() << std::endl;
			exit(-1);
		}

		std::string s(command);
		if(executeCommand) {
			s = ":" + s;
		}

		while(true) {
			if(s.length() == 0) {
				break;
			}

			if(s.length() > MAX_COMMAND_SIZE-1) {
				cout << "You entered a command that is too long! Max command size is " << MAX_COMMAND_SIZE-1 << " characters!" << std::endl;
				break;
			}

			s += '\n';
			char b[MAX_COMMAND_SIZE + 1];
			strncpy(b, s.c_str(), s.length());
			b[MAX_COMMAND_SIZE] = '\0';
			if(send(socketHandle, b, s.length(), 0) == -1) {
				cout << "Error while sending to unix socket connection!" << std::endl;
				break;
			}
			else {
				break;
			}
		}

		close(socketHandle);
		return 0;
	}

	if (lockf(lfp,F_TLOCK,0)<0) { // Another buckey process is running, turn into a client
		syslog(LOG_INFO, "Another buckey process is running! Turning into command line mode.");
		cout << "It appears that another Buckey process is running, this process will become a command line to communicate with the other process!" << std::endl;

		//Open the unix socket
		int socketHandle;
		struct sockaddr_un remote;

		socketHandle = socket(AF_UNIX, SOCK_STREAM, 0);
		if(socketHandle == -1) {
			syslog(LOG_ERR, "Error opening client unix socket!");
			std::cout << "Error opening client unix socket!" << std::endl;
			exit(-1);
		}

		remote.sun_family = AF_UNIX;
		std::string socketPath = coreConfig["unix-socket-path"].as<std::string>();
		strcpy(remote.sun_path, socketPath.c_str());

		int len = strlen(remote.sun_path) + sizeof(remote.sun_family);

		int res = connect(socketHandle, (struct sockaddr *)&remote, len);
		if(res == -1) {
			syslog(LOG_ERR, "Error connecting to server unix socket!");
			std::cout << "Error connecting to server unix socket! " << coreConfig["unix-socket-path"].as<std::string>() << std::endl;
			exit(-1);
		}

		std::string s;
		while(true) {
			s = "";
			getline(cin, s);
			if(s.length() == 0) {
				continue;
			}

			if(s.length() > MAX_COMMAND_SIZE-1) {
				cout << "You entered a command that is too long! Max command size is " << MAX_COMMAND_SIZE-1 << " characters!" << std::endl;
				continue;
			}

			if(s == "exit") {
				break;
			}
			else {
				s += '\n';
				char b[MAX_COMMAND_SIZE + 1];
				strncpy(b, s.c_str(), MAX_COMMAND_SIZE);
				b[MAX_COMMAND_SIZE] = '\0';
				if(send(socketHandle, b, s.length(), 0) == -1) {
					cout << "Error while sending to unix socket connection!" << std::endl;
					break;
				}
			}
			cout << "Command: " << s << std::endl;
		}

		close(socketHandle);
	}
	else { // This is the only buckey process running, make a new buckey or make a new daemon

		if(makeDaemon) {
			daemonize();
		}
		else {
			makeBuckey();

			std::string s;
			struct pollfd pollstruct;
			pollstruct.fd = 0; //stdin file descriptor
			pollstruct.events = POLLIN;
			while(!buckey->isKilled() && buckey->isRunning()) {
				s = "";

				if(poll(&pollstruct, 1, 500) > 0) {
					getline(cin, s);
				}
				else {
					continue;
				}

				if(!buckey->isKilled()) {
					if(s != "") {
						buckey->passInput(s);
					}
				}
				else {
					std::cout << "Buckey has exited! Command not sent." << std::endl;
					break;
				}
			}
			buckey->requestStop();
			delete buckey;
		}
	}

	return 0;
}

/*! \page lifecycle Buckey's Lifecycle
	\p An important aspect of C++ (and C) programming is memory management. Buckey also makes heavy use of asynchronous methods (such as std::thread).
	This poses many problems as child threads must join the main thread when Buckey exits, and resources that multiple threads access must be shielded with mutexes and atomics.
	This also means that child threads must free their allocated memory unless it becomes a memory leak (even if the kernel sandboxes the process and force free's the memory, it is stll bad practice).

	When a request for Buckey to stop (such as Buckey::requestStop()), Buckey's protected field atomic<bool> killed is set to true.
	This protected Buckey::killed field is public accessible through the bool Buckey::isKilled() method.
	When creating child threads that have a long task or an infinitely long loop, have the loop check the isKilled() method periodically to avoid exceptions being thrown because your thread is forced to terminate and does not join the main thread.
	Yes, you could detach your thread, but unless you realize that you will never have access to Buckey's methods and data, it is very inadvisable to do so.

	When the Buckey object is destructed, it sets the atomic<bool> killed to true and then saves the currently enabled services and modes to the services.enabled and modes.enabled core config files and then disables all Modes and then disables all Services.
	Your Modes and Services should already be ready to be disabled as they should have been listening to Buckey's isKilled() method and all child threads should have ended. Your service's disable function should join any child threads that have exited. Exiting a child thread is not enough, you must join it.

	When a class extends the EventSource class, the EventSource class takes care of its own memory and thread management on construction and destruction.
	When triggering events with the triggerEvents(std::string type, EventData * data) method, there is no need to free/delete the EventData * data that you pass to the function.
	Deleting the EventData that you pass to the method prematurely will result in segfaults as the event handler methods will be accessing memory that has been unallocated!
	Because of this, the EventSource class internally frees/deletes the EventData that you pass to it when calling triggerEvents().

*/
