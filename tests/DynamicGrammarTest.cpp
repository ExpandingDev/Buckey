#include "grammar.h"
#include "DynamicGrammar.h"

#include <iostream>
#include <string>

using namespace std;

int main() {
	cout << "Dynamic Grammar Test" << endl;
    ifstream f("tests/dynamic.gram", ios_base::in);
	if(f.is_open()) {
		DynamicGrammar g(f);
		for(string s : g.listVariables()) {
			cout << "Found variable: " << s << endl;
		}

		g.setVariable("name", "Buckey");

		shared_ptr<AlternativeSet> modeList(new AlternativeSet());
		modeList->addChild("calculator");
		modeList->addChild("settings");
		g.setVariable("modeList", modeList);

		g.setVariable("THISSHOULDFAILSILENTLY", "UHOH");

		cout << "New Grammar:" << endl << g.getText() << endl;
        ofstream o("test-output.gram", ios_base::out);
        if(o.is_open()) {
			g.writeGrammar(o);
        }
        o.close();
	}
	else {
		cout << "Failed to open the test grammar file!"	<< endl;
		return -1;
	}
	f.close();
}
