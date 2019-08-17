#ifndef SERVICE_H_INCLUDED
#define SERVICE_H_INCLUDED
#include <atomic>

#include "yaml-cpp/yaml.h"

#include "cppfs/FileHandle.h"
#include "cppfs/fs.h"

///\brief All possible states that a Service may be in
enum class ServiceState {
	///Service has started up, and is currently running. Modes and Services may safely make calls to this Service
	RUNNING,
	///Service is starting up but is not fully functional yet, Modes and other Services should avoid making any calls to this service until it is RUNNING
	STARTING,
	///Service is not running, calls should not be made to this Service.
	STOPPED,
	///Service encountered an error forcing it to stop running or abort the stop process
	ERROR,
	///A service with the specified name is not registered with Buckey
	SERVICE_NOT_REGISTERED
};

///\brief An abstract class that Services should extend and implement.
///
///Services are core components of Buckey that provide additional input or output methods.
///Services may be larger and contain their own threads.
///Services may be enabled or disabled by the user, but the user may also build Buckey with select Services available in order to conserve space/efficiency.
class Service {
	public:
		Service();
		virtual ~Service();

		//General
		///\brief Returns the name of the Service
		///\return std::string The name of the Service
		virtual std::string getName() const = 0;
		///\brief Sets the current configuration directory. Should be called before enable()
		///\param configDir [in] The cppfs::FileHandle to set the current configuration directory to.
		void setConfigDir(cppfs::FileHandle cDir);
		///\brief Sets the current assets directory. Should be called before enable()
		///\param assetsDir [in] The cppfs::FileHandle to set the current assets directory to.
		void setAssetsDir(cppfs::FileHandle aDir);

		// Control
		virtual void start() = 0;
		virtual void stop() = 0;
		///\brief Reloads (stops and starts) the Service.
		virtual void reload();

		//Config/startup
		///\brief Called on the start of Buckey if the service does not have a configuration directory
		virtual void setupConfig(cppfs::FileHandle cDir) = 0;
		///\brief Called on the start of Buckey if the service does not have an assets directory
		virtual void setupAssets(cppfs::FileHandle aDir) = 0;

		// State
		///\brief Returns the ServiceState of the Service
		///\return ServiceState The current state of the service
		virtual ServiceState getState() const;
		virtual std::string getStatusMessage() const;
		///\brief Called by Buckey when the service is Registered, changes ServiceState from NOT_REGISTERD to DISABLED.
		void registered();

		//Utility
		///\brief Returns a cut and paste status message using the ServiceState and service name
		static std::string generateNormalStatusMessage(ServiceState state, const std::string & serviceName);

	protected:
		///The current assets directory.
		cppfs::FileHandle assetsDir;
		///The current configuration directory.
		cppfs::FileHandle configDir;
		///The current ServiceState of the Service.
		std::atomic<ServiceState> state;
		///Helper function that sets the current ServiceState of the Service.
		void setState(ServiceState s);
};

#endif // SERVICE_H_INCLUDED
