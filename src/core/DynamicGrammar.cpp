#include "DynamicGrammar.h"

unsigned long DynamicGrammar::nextID = 0;
std::mutex DynamicGrammar::idLock;

DynamicGrammar::DynamicGrammar() : Grammar() {
	idLock.lock();
	id = nextID;
	nextID++;
	idLock.unlock();

	//Find all of the dynamic rules
	for(shared_ptr<Rule> r : rules) {
			Expansion * e = r->getRuleExpansion().get();
			walkoverExpansion(this, e);
			if(e->hasChild()) {
			for(unsigned int i = 0; i < e->childCount(); i++) {
				walkoverExpansion(this, e->getChild(i).get());
			}
		}
	}

	//Make rules for all of them
	for(std::string s : variables) {
		shared_ptr<Rule> r(new Rule(s, false, shared_ptr<Expansion>(new Token("default value"))));
		addRule(r);
	}
}

DynamicGrammar::DynamicGrammar(std::istream & inputStream) : Grammar(inputStream) {
	idLock.lock();
	id = nextID;
	nextID++;
	idLock.unlock();

	//Find all of the dynamic rules
	for(shared_ptr<Rule> r : rules) {
			Expansion * e = r->getRuleExpansion().get();
			walkoverExpansion(this, e);
			if(e->hasChild()) {
			for(unsigned int i = 0; i < e->childCount(); i++) {
				walkoverExpansion(this, e->getChild(i).get());
			}
		}
	}

	//Make rules for all of them
	for(std::string s : variables) {
		shared_ptr<Rule> r(new Rule(s, false, shared_ptr<Expansion>(new Token("default value"))));
		addRule(r);
	}
}

DynamicGrammar::DynamicGrammar(std::istream * inputStream) : Grammar(inputStream) {
	idLock.lock();
	id = nextID;
	nextID++;
	idLock.unlock();

	//Find all of the dynamic rules
	for(shared_ptr<Rule> r : rules) {
			Expansion * e = r->getRuleExpansion().get();
			walkoverExpansion(this, e);
			if(e->hasChild()) {
			for(unsigned int i = 0; i < e->childCount(); i++) {
				walkoverExpansion(this, e->getChild(i).get());
			}
		}
	}

	//Make rules for all of them
	for(std::string s : variables) {
		shared_ptr<Rule> r(new Rule(s, false, shared_ptr<Expansion>(new Token("default value"))));
		addRule(r);
	}
}

DynamicGrammar::~DynamicGrammar()
{

}

void DynamicGrammar::resetNextID() {
	idLock.lock();
	nextID = 0;
	idLock.unlock();
}

shared_ptr<Expansion> DynamicGrammar::getVariable(std::string variableName) {
	std::string n = variableName + getSuffix();
	return getRule(n)->getRuleExpansion();
}

void DynamicGrammar::listVariables(std::vector<std::string> & output) {
	for(std::string s : variables) {
		output.push_back(s);
	}
}

void DynamicGrammar::walkoverExpansion(DynamicGrammar * g, Expansion * e) {
	if(e->getType() == RULE_REFERENCE) {
		RuleReference * r = (RuleReference *) e;
		std::string n = r->getRuleName();
		if(StringHelper::stringStartsWith(n, "$")) {
			r->setRuleName(r->getRuleName() + getSuffix()); // Rename the rule name with the name of the grammar appended to it
			g->variables.push_back(r->getRuleName());
		}
		return;
	}

	if(e->childCount() != 0) {
		for(unsigned int i = 0; i < e->childCount(); i++) {
			g->walkoverExpansion(g, e->getChild(i).get());
		}
	}
}

void DynamicGrammar::setVariable(std::string name, std::string value) {
	shared_ptr<Rule> r = getRule("$" + name + getSuffix());
	if(r) {
		shared_ptr<Expansion> text(new Token(value));
		shared_ptr<Expansion> container(new RequiredGrouping(text));
		r->setRuleExpansion(container);
	}
}

void DynamicGrammar::setVariable(std::string name, std::shared_ptr<Expansion> value) {
	shared_ptr<Rule> r = getRule("$" + name + getSuffix());
	if(r) {
		shared_ptr<Expansion> container(new RequiredGrouping(value));
		r->setRuleExpansion(container);
	}
}

std::vector<std::string> DynamicGrammar::listVariables() {
	return variables;
}

std::string DynamicGrammar::getSuffix() {
	return "-" + getName() + std::to_string(id);
}
