#ifndef HYPOTHESISEVENTDATA_H
#define HYPOTHESISEVENTDATA_H
#include <string>

#include <core/EventData.h>

///EventData extended class that holds a std::string representing the hypothesis generated by a speech recognition Service.
class HypothesisEventData : public EventData
{
    public:
        HypothesisEventData(std::string h = "");
        virtual ~HypothesisEventData();
        ///Returns the stored hypothesis string
        std::string getHypothesis() const;
    protected:
    	///The stored hypothesis string
        std::string hypothesis;
};

#endif // HYPOTHESISEVENTDATA_H