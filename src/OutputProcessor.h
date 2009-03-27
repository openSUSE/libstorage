#ifndef OUTPUT_PROCESSOR_H
#define OUTPUT_PROCESSOR_H

#include "y2storage/StorageInterface.h"

namespace storage
{

    class OutputProcessor
    {
    public:
	OutputProcessor() {}
	virtual ~OutputProcessor() {}
	virtual void reset() = 0;
	virtual void finished() = 0;
	virtual void process(const string& txt, bool stderr) = 0;
    };


class ScrollBarHandler : public OutputProcessor
    {
    public:
	ScrollBarHandler( const string& sid, storage::CallbackProgressBar cb ) 
	      { id=sid; first=true; callback=cb; cur=0; max=100; }
	virtual ~ScrollBarHandler() {}
	virtual void reset() { first=true; cur=0; }
	virtual void finished() { setCurValue( max ); }
	virtual void process( const string& txt, bool stderr );
	void setMaxValue( unsigned val ) { max=val; }
	unsigned getMaxValue() { return( max ); }
	void setCurValue( unsigned val ); 
	unsigned getCurValue() {  return( cur ); }

    protected:
	unsigned long max;
	unsigned long cur;
	bool first;
	string id;
	storage::CallbackProgressBar callback;
    };

class Mke2fsScrollbar : public ScrollBarHandler
    {
    public:
	Mke2fsScrollbar( storage::CallbackProgressBar cb ) :
	    ScrollBarHandler( "format", cb ) { done=false; }
	virtual void process( const string& txt, bool stderr );
    protected:
	string seen;
	bool done;
    };

class ReiserScrollbar : public ScrollBarHandler
    {
    public:
	ReiserScrollbar( storage::CallbackProgressBar cb ) :
	    ScrollBarHandler( "format", cb ) { max=100; }
	virtual void process( const string& txt, bool stderr );
    protected:
	string seen;
    };

class DasdfmtScrollbar : public ScrollBarHandler
    {
    public:
	DasdfmtScrollbar( storage::CallbackProgressBar cb ) :
	    ScrollBarHandler( "dasdfmt", cb ) { max=100; max_cyl=cur_cyl=0; }
	virtual void process( const string& txt, bool stderr );
    protected:
	string seen;
	unsigned long cur_cyl;
	unsigned long max_cyl;
    };

}

#endif
