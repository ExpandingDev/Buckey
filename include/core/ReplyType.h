#ifndef REPLYTYPE_H
#define REPLYTYPE_H


enum class ReplyType
{
	///This message will be displayed on the console, spoken, and shown on any available GUI
    CONVERSATION,
    ///This message will be displayed on the console, spoken, and shown on any available GUI, as well as triggering a popup/toast notification if available
    ALERT,
    ///This message will not be displayed on the console, nor will it be spoken. May be displayed by the GUI and may trigger a notification if available. Useful for altering the user when services are loaded or have data for the user
    STATUS,
    ///This message will be displayed on the console, will not be spoken, will also be logged.
    CRITICAL,
    ///This message should only displayed on the console, either verbose or for debugging. Will not be spoken.
    CONSOLE,
    ///This message should be displayed, will not be logged, will be spoken, and may result in a GUI popup
    PROMPT
};

#endif // REPLYTYPE_H
