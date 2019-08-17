#ifndef DYNAMICGRAMMAR_H
#define DYNAMICGRAMMAR_H

#include "grammar.h"
#include "expansion.h"
#include "StringHelper.h"

#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <mutex>

///\brief An extended Grammar class that allows for variable rule references.
///
///		An extended Grammar class that allows for variable rule references.
///		Variable rule references should start with a $ (like PHP or Bash variables).
///		The value of these variable rules can be set by calling the setVariable() method.
class DynamicGrammar : public Grammar
{
	public:
		DynamicGrammar();
		DynamicGrammar(std::istream & inputStream);
		DynamicGrammar(std::istream * inputStream);
		virtual ~DynamicGrammar();

		std::string getSuffix();

		void listVariables(std::vector<std::string> & output);

        void setVariable(std::string variableName, std::string value);
        void setVariable(std::string variableName, std::shared_ptr<Expansion> expansion);

        shared_ptr<Expansion> getVariable(std::string variableName);

        std::vector<std::string> listVariables();

        /// Kinda a hack, but should be called once the risk of collision between IDs is low and running out of space for IDs (ie, program has been running for months and used a TON of dynamic grammars)
        static void resetNextID();

	protected:
		///List of all the variable rules in the grammar
		std::vector<std::string> variables;

		///Called when constructing the DynamicGrammar after the base Grammar class parses all of the Expansions
		void walkoverExpansion(DynamicGrammar *, Expansion * e);

		/// This is a unique ID to ensure that when dynamic grammars are fused together the variable names do not conflict
		unsigned long id;

		///ID of the next DynamicGrammar constructed. Do not access this unless you lock the idLock mutex!
		static unsigned long nextID;

		/// This must be locked when reading or writing to the nextID variable. This is to ensure that two dynamic grammars never attain the same id by running on different threads.
		static std::mutex idLock; };

#endif // DYNAMICGRAMMAR_H
