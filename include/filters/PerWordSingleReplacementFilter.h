#ifndef PerWordSingleReplacementFilter_H
#define PerWordSingleReplacementFilter_H
#include <utility>
#include <fstream>
#include <iostream>
#include <regex>
#include <vector>

#include "yaml-cpp/yaml.h"

#include <filters/TextFilter.h>
#include <filters/StringHelper.h>

///\brief A word-by-word single replacement TextFilter.
/**
  * PerWordSingleReplacementFilter is a word-by-word single replacement TextFilter.
  * This means that when calling PerWordSingleReplacementFilter::filterText() , each word in the input string is matched against the regex list that the PerWordSingleReplacementFilter has.
  * If a match is found, then the word is replaced with the respective replacement string specified by the replacement dictionary
  *
  */
class PerWordSingleReplacementFilter : public TextFilter
{
    protected:
        std::ifstream replacementDictionary;
        std::vector<std::pair<std::regex,std::string>> replacementRules;

    public:
        /**
          * Default constructor, really should never be called
          */
        PerWordSingleReplacementFilter();

        /**
          * Constructor that parses in the dictionary
          * \param [in] dictionaryPath Path to the replacement dictionary file to use
          */
        PerWordSingleReplacementFilter(std::string dictionaryPath);
        std::string filterText(std::string input);
        ~PerWordSingleReplacementFilter();

        const static std::string dictionaryDefinitionDelimiter;
};

#endif // PerWordSingleReplacementFilter_H
