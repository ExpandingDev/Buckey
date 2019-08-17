#include "PerWordSingleReplacementFilter.h"
#include <iostream>
#include <string>

using namespace std;

int main() {
    cout << "Please type in some text that will be replaced with a per word single replacement filter" << endl;
    string input;
    getline(cin, input);
    PerWordSingleReplacementFilter filter("tests/numbers.pwsr");
    string output = filter.filterText(input);
    cout << output << endl;
}
