#include "PromptResult.h"

PromptResult::PromptResult()
{
	isTimedOut = true;
}

PromptResult::PromptResult(void * data) {
	isTimedOut = false;
	d = data;
}

PromptResult::~PromptResult()
{
	delete d;
}

void * PromptResult::releaseData() {
	void * a = d;
	d = nullptr;
	return a;
}

void * PromptResult::getResult() {
	return d;
}

bool PromptResult::timedOut() const {
	return isTimedOut;
}
