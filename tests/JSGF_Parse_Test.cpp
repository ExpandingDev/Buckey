#include <iostream>
#include <string>
#include <rule.h>
#include <grammar.h>

using namespace std;

int main() {
	cout << "JSGF Parse Test" << endl;
	Grammar g;
	Grammar::parseGrammarFromString("grammar testing; \
									public <command> = [<greetings>] <mode_keywords>; \
									<greetings> = (hello | hi | can you | hey [so | can you]) [please] {polite}; \
									<mode_keywords> = calculate {calc} <calc_commands> | find {find} <find_commands>; \
									<calc_commands> = <digits>+ (<operator> <digits>+)+; \
									<operator> = add | subtract | multiply | divide; \
									<digits> = zero | one | two | three | four | five | six | seven | eight | nine; \
									<find_commands> = the time {time} | my location {me};", g);


	cout << "Parsed" << endl;
	cout << "================================" << endl;
	((AlternativeSet *) (g.getRule("digits")->getRuleExpansion().get()))->addChild("test");
	cout << g.getText() << endl;
	cout << "================================" << endl;
	cout << "Grammar Name: " << g.getName() << endl;

	cout << "Done" << endl;
}
