#ifndef MODE_H
#define MODE_H
#include <string>
#include <vector>
#include <atomic>

#include "cppfs/fs.h"
#include "cppfs/FileHandle.h"

#include "grammar.h"

///String that is prepended to all Mode grammars as they are added to the root grammar
#define MODE_TAG_PREFIX "-_buckey_mode_-"

///\brief All possible states that a Mode can be in
enum class ModeState {
	///Mode is running
	STARTED,
	///Mode is not running
	STOPPED,
	///Mode has encountered an error.
	ERROR,
	///Mode does not exist, returned when no Mode can be found with a specified name
	NOT_LOADED
};

///\brief An abstract class for Modes to extend.
///
///Modes are smaller features/programs that the user can access through Buckey using the available input and output services.
///Should be event oriented and take up less space than services.
class Mode
{
	public:
		Mode();
		virtual ~Mode();

		//Config/setup
		///Sets the configuration directory for the Mode. Should be called before enabling the Mode.
		void setConfigDir(cppfs::FileHandle cDir);

		///Sets the asset directory for the Mode. Should be called before enabling the Mode.
		void setAssetsDir(cppfs::FileHandle aDir);

		/**
		  * Called by Buckey when registering all Modes if the config directory for this Mode cannot be found.
		  * Add in all of your default files and config here.
		  * \param cDir [in] cppfs::FileHandle to the created config directory
		  */
		virtual void setupConfigDir(cppfs::FileHandle cDir) = 0;

		/**
		  * Called by Buckey when registering all Modes if the assets directory for this Mode cannot be found.
		  * Add in all of your default files and assets here.
		  * \param aDir [in] cppfs::FileHandle to the created asset directory
		  */
		virtual void setupAssetsDir(cppfs::FileHandle aDir) = 0;

		//Input
		/**
		  * Called by Buckey::passCommand when the input matches this Mode's grammar.
		  * \param command [in] The command that was entered by the user
		  * \param tags [in] A vector of matching tags from the root grammar
		  */
		virtual void input(std::string & command, std::vector<std::string> & tags) = 0;

		// State
		///Returns the current ModeState of the Mode
		ModeState getState() const;

		///\brief Called when enabling the Mode
		///Please put all event listener registration in here and store the event handler IDs somewhere to unregister them when disable() is called
		virtual void start() = 0;

		///\brief Called when disabling the Mode
		///Close files terminate threads, you should also unregister/unset any event listeners you have attached during enable()
		virtual void stop() = 0;

		///Returns the name of the Mode
		std::string getName() const;

	protected:
		///Sets the ModeState of this mode to the specified state.
		void setState(const ModeState newState);

		///Holds the current ModeState of this mode, set using setState please.
		std::atomic<ModeState> state;

		///cppfs::FileHandle of the current assets directory
		cppfs::FileHandle assetsDir;

		///cppfs::FileHandle of the current config directory
		cppfs::FileHandle configDir;

		///The name of this mode
		std::string name;
};

#endif // MODE_H
