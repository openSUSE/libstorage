#ifndef OUTPUT_PROCESSOR_H
#define OUTPUT_PROCESSOR_H


#include "storage/StorageInterface.h"


namespace storage
{

    class OutputProcessor
    {
    public:
	OutputProcessor() {}
	virtual ~OutputProcessor() {}
	virtual void reset() = 0;
	virtual void finish() = 0;
	virtual void process(const string& txt, bool stderr) = 0;
    };


    class ProgressBar : public OutputProcessor
    {
    public:
	ProgressBar(const string& id, CallbackProgressBar callback)
	    : id(id), callback(callback), first(true), max(100), cur(0)
	{}

	virtual ~ProgressBar() {}

	virtual void reset() { first = true; cur = 0; }
	virtual void finish() { setCurValue(max); }
	virtual void process(const string& txt, bool stderr) {}

	void setMaxValue(unsigned val) { max = val; }
	unsigned getMaxValue() const { return max; }
	void setCurValue(unsigned val);
	unsigned getCurValue() const { return cur; }

    protected:
	const string id;
	const CallbackProgressBar callback;

	bool first;

    private:
	unsigned long max;
	unsigned long cur;
    };


    class Mke2fsProgressBar : public ProgressBar
    {
    public:
	Mke2fsProgressBar(CallbackProgressBar callback)
	    : ProgressBar("format", callback)
	{
	    setMaxValue(100);
	    done = false;
	}

	virtual void process(const string& txt, bool stderr);

    protected:
	string seen;
	bool done;
    };


    class ReiserProgressBar : public ProgressBar
    {
    public:
	ReiserProgressBar(CallbackProgressBar callback)
	    : ProgressBar("format", callback)
	{
	    setMaxValue(100);
	}

	virtual void process(const string& txt, bool stderr);

    protected:
	string seen;
    };


    class DasdfmtProgressBar : public ProgressBar
    {
    public:
	DasdfmtProgressBar(CallbackProgressBar callback)
	    : ProgressBar("dasdfmt", callback)
	{
	    setMaxValue(100);
	    max_cyl = cur_cyl = 0;
	}

	virtual void process(const string& txt, bool stderr);

    protected:
	string seen;
	unsigned long max_cyl;
	unsigned long cur_cyl;
    };

}


#endif
