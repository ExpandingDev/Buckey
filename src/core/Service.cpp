#include "Service.h"

///On creation every service is in the SERVICE_NOT_REGISTERED state.
Service::Service() {
	setState(ServiceState::SERVICE_NOT_REGISTERED);
}

ServiceState Service::getState() const {
	return state;
}

void Service::setState(ServiceState s) {
	state.store(s);
}

void Service::setConfigDir(cppfs::FileHandle cDir) {
	configDir = cDir;
}

void Service::setAssetsDir(cppfs::FileHandle aDir) {
	assetsDir = aDir;
}

void Service::reload() {
	stop();
	enable();
}

///Called once the service is registered.
void Service::registered() {
	setState(ServiceState::DISABLED);
}

std::string Service::getStatusMessage() const {
	return generateNormalStatusMessage(state, this->getName());
}

std::string Service::generateNormalStatusMessage(ServiceState state, const std::string & serviceName) {
	switch(state) {
		case ServiceState::STARTING:
			return serviceName + " service is starting up.";
			break;
		case ServiceState::RUNNING:
			return serviceName + " service is running.";
			break;
		case ServiceState::STOPPED:
			return serviceName + " service is stopped.";
			break;
		case ServiceState::ERROR:
			return serviceName + " service has an error!";
			break;
		case ServiceState::DISABLED:
			return serviceName + " service is disabled.";
			break;
		case ServiceState::SERVICE_NOT_REGISTERED:
			return serviceName + " service is not registered.";
			break;
		default:
			return "Default status message for " + serviceName + " service.";
			break;
	}
}

Service::~Service() {

}
