#include "HypothesisEventData.h"

HypothesisEventData::HypothesisEventData(std::string h)
{
    hypothesis = h;
}

HypothesisEventData::~HypothesisEventData()
{
    //dtor
}


std::string HypothesisEventData::getHypothesis() const {
    return hypothesis;
}
