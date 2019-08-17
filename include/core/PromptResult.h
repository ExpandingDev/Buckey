#ifndef PROMPTRESULT_H
#define PROMPTRESULT_H


class PromptResult
{
	public:
		PromptResult();
		PromptResult(void * data);
		bool timedOut() const;
		void * getResult();
		void * releaseData();
		virtual ~PromptResult();

	protected:
		bool isTimedOut;
		void * d;

	private:
};

#endif // PROMPTRESULT_H
