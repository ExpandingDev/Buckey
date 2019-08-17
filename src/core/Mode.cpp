#include "Mode.h"
#include "Buckey.h"

Mode::Mode()
{
	state = ModeState::NOT_LOADED;
}

ModeState Mode::getState() const {
	return state;
}

void Mode::setState(const ModeState s) {
	state.store(s);
}

void Mode::setConfigDir(cppfs::FileHandle cDir) {
	configDir = cDir;
}

void Mode::setAssetsDir(cppfs::FileHandle aDir) {
	assetsDir = aDir;
}

std::string Mode::getName() const
{
	return name;
}

Mode::~Mode()
{

}
