#ifndef TEXTFILTER_H
#define TEXTFILTER_H
#include <string>

#include "yaml-cpp/yaml.h"

///\brief An abstract class that should be extended by TextFilter implementation
class TextFilter
{
    protected:
        /// YAML configuration for this TextFiler
        YAML::Node configNode;

    public:
        TextFilter();

        /**
          * Constructor that parses in the text filter config
          * \param [in] configPath Path to the YAML configuration file for the TextFilter
          */
        TextFilter(std::string configPath);

        /**
          * Function that runs the input string against the replacement/manipulation rules of the TextFilter and returns the resulting string.
          * \param [in] input String that is going to be copied and filtered
          * \return std::string The resulting string after the input string was manipulated
          */
        virtual std::string filterText(std::string input) = 0;
        virtual ~TextFilter();
};

#endif // TEXTFILTER_H
