#include "filters/StringHelper.h"
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
    std::string replaceFirst(std::string s, std::string re, std::string replacement)
    {
        std::regex reg (re);
        return regex_replace(s, reg, replacement, std::regex_constants::format_first_only);
    }

    /**
      * Static helper function that replaces all occurrences of regex re in std::string s with the replacement string
      * \param [in] s string that is being modified
      * \param [in] re A string representing a C regex that will be matches against string s
      * \param [in] replacement A string that will replace the first match of re against string s
      * \return string The modified string with all occurrences of re replaced with replacement
      */
    std::string replaceAll(std::string s, std::string re, std::string replacement)
    {
        std::regex reg (re);
        return regex_replace(s, reg, replacement);
    }

    ///Compliments to: https://stackoverflow.com/questions/16749069/c-split-string-by-regex for the below code
    std::vector<std::string> splitString(const std::string & s, std::string rgx_str)
    {
        std::vector<std::string> elems;

        std::regex rgx (rgx_str);

        std::sregex_token_iterator iter(s.begin(), s.end(), rgx, -1);
        std::sregex_token_iterator end;

        while (iter != end)
        {
            elems.push_back(*iter);
            ++iter;
        }

        return elems;
    }

    bool stringContains(std::string part, std::string search)
    {
        size_t p = part.find(search);
        return p != std::string::npos;
    }

    bool stringStartsWith(std::string s, std::string test)
    {
        return s.find(test) == 0;
    }

    bool stringEndsWith(std::string s, std::string test)
    {
        return (s.find(test) == s.length() - test.length()) && s.find(test) != std::string::npos;
    }

    ///Thanks for this trimming function: https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring

	// trim from start
	inline std::string &ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(),
				std::not1(std::ptr_fun<int, int>(std::isspace))));
		return s;
	}

	// trim from end
	inline std::string &rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(),
				std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
		return s;
	}


	std::string trimString(std::string input)
	{
		return ltrim(rtrim(input));
	}

	std::string concatenateStringVector(std::vector<std::string> strings, char delim) {
		if(strings.size() > 0) {
			std::string s = strings[0];
			for(std::vector<std::string>::iterator i = strings.begin() + 1; i != strings.end(); i++) {
				s += delim;
				s += *i;
			}
			return s;
		}
		return "";
	}

}
