#include "EchoMode.h"
#include "Buckey.h"
#include "PromptResult.h"

std::atomic<bool> EchoMode::instanceSet(false);
EchoMode * EchoMode::instance;

EchoMode * EchoMode::getInstance() {
	if(!instanceSet.load()) {
		instance = new EchoMode();
		instanceSet.store(true);
	}
	return instance;
}

void EchoMode::setupAssetsDir(cppfs::FileHandle aDir) {

}

void EchoMode::setupConfigDir(cppfs::FileHandle cDir) {

}

void EchoMode::enable() {
	Buckey::getInstance()->addModeToRootGrammar(this, grammar);
	state = ModeState::ENABLED;
}

void EchoMode::disable() {
	Buckey::getInstance()->removeModeFromRootGrammar(this, grammar);
	state = ModeState::DISABLED;
}

///TODO: Main thread is being hung up because passing input to a mode is synchronous and it waits for the mode to return, which it doesn't because it is waiting for input from the user, and the user cannot enter input because the CLI is hung waiting for it to return. Make everything go through inputQue and check inputQue in another thread separate from passInput().
void EchoMode::input(std::string & command, std::vector<std::string> & tags) {
	Buckey * buckey = Buckey::getInstance();
	buckey->reply("pong", ReplyType::CONVERSATION);
	buckey->startConversation();

	PromptResult * p = Buckey::getInstance()->promptConfirmation("Are you sure?");
	if(p->timedOut()) {
		buckey->reply("Result timed out", ReplyType::CONVERSATION);
	}
	else {
		buckey->reply("Result not timed out", ReplyType::CONVERSATION);
	}

	buckey->endConversation();
}

EchoMode::EchoMode()
{
	name = "echo";
	grammar = new DynamicGrammar();
	Grammar::parseGrammarFromString("grammar echo;\npublic <command> = echo;", *grammar);
	setState(ModeState::DISABLED);
}

EchoMode::~EchoMode()
{
	delete grammar;
}
