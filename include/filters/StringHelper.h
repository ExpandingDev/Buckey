#ifndef STRINGHELPER_H
#define STRINGHELPER_H
#include <regex>
#include <list>
#include <string>
#include <vector>

namespace StringHelper
{
    /**
      * Static helper function that replaces the first occurrence of regex re in string s with the replacement string
      * \param [in] s string that is being modified
      * \param [in] re A string representing a C regex that will be matches against string s
      * \param [in] replacement A string that will replace the first match of re against string s
      * \return string The modified string with the first occurrence of re replaced with replacement
      */
    std::string replaceFirst(std::string s, std::string re, std::string replacement);

    /**
      * Static helper function that replaces all occurrences of regex re in std::string s with the replacement string
      * \param [in] s string that is being modified
      * \param [in] re A string representing a C regex that will be matches against string s
      * \param [in] replacement A string that will replace the first match of re against string s
      * \return string The modified string with all occurrences of re replaced with replacement
      */
    std::string replaceAll(std::string s, std::string re, std::string replacement);

    ///Compliments to: https://stackoverflow.com/questions/16749069/c-split-string-by-regex for the below code
    std::vector<std::string> splitString(const std::string & s, std::string rgx_str);

    bool stringContains(std::string part, std::string search);

    bool stringStartsWith(std::string s, std::string test);

    bool stringEndsWith(std::string s, std::string test);

    std::string trimString(std::string input);

    ///Concatenates all of the strings in the vector together with the specified delimiter between
    std::string concatenateStringVector(std::vector<std::string> strings, char delim = ' ');
}

#endif // STRINGHELPER_H
