#include "filters/PerWordSingleReplacementFilter.h"
#include <iostream>
const std::string PerWordSingleReplacementFilter::dictionaryDefinitionDelimiter = "\"=\"";

PerWordSingleReplacementFilter::PerWordSingleReplacementFilter() { }

PerWordSingleReplacementFilter::PerWordSingleReplacementFilter(std::string configPath)
{
    replacementDictionary.open(configPath);

    std::string line;
    // Iterate through each line in the dictionary
    while(std::getline(replacementDictionary, line)) {
        line = StringHelper::trimString(line);
        if(line.size() == 0) {
            continue;
        }

        if(StringHelper::stringStartsWith(line, "#")) {
            continue;
            // Its a comment so skip over it
        }
        else {
            std::vector<std::string> parts = StringHelper::splitString(line, dictionaryDefinitionDelimiter);
            std::string replacementRule = StringHelper::replaceFirst(parts[0],"\"", "");
            std::string replacementString = parts[1];
            replacementString.pop_back(); // Removes the last char, should be the "

            try {
                std::regex newRule(replacementRule);
                replacementRules.push_back(std::make_pair(newRule, replacementString));
            }
            catch (std::regex_error & e) {
                std::cerr << "Error while parsing " << replacementRule << " into a regex!" << std::endl;
            }
        }
    }
}

PerWordSingleReplacementFilter::~PerWordSingleReplacementFilter() {}

std::string PerWordSingleReplacementFilter::filterText(std::string input) {
    std::string output;
    std::vector<std::string> words = StringHelper::splitString(input, " ");
    bool matched = false;

    for(std::string s : words) {
        matched = false;
        for(std::pair<std::regex,std::string> p : replacementRules) {
            if(std::regex_match(s, p.first)) {
                output += p.second + " ";
                matched = true;
                break;
            }
        }
        if(!matched) {
            output += s + " ";
        }
    }

    return StringHelper::trimString(output);
}
