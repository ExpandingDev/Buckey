#ifndef EVENTDATA_H
#define EVENTDATA_H
#include <string>

///This is a class that should hold data about an event that was triggered. It is highly recommended to extend this class for specific use cases.
class EventData
{
    public:
    	///Construct holding no data
        EventData();
        ///Construct holding a string as data
        EventData(std::string t);
        ///Construct holding a bool as data
        EventData(bool k);
        ///Construct holding an int as data
        EventData(int i);
        virtual ~EventData();

        ///Returns the stored boolean value, if there was one
        bool getBool();
        ///Returns the stored string value, if there was one
        std::string getString();
        ///Returns the stored integer value, if there was one
        int getInt();

    protected:
    	///The stored boolean value
        bool b;
        ///The stored integer value
        int i;
        ///The stored string value
        std::string s;
};

#endif // EVENTDATA_H
