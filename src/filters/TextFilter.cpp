#include "filters/TextFilter.h"

TextFilter::TextFilter() {

}

TextFilter::TextFilter(std::string configPath)
{
    configNode = YAML::LoadFile(configPath);
}

TextFilter::~TextFilter()
{
    //dtor
}

std::string TextFilter::filterText(std::string input) {
    return input;
}
