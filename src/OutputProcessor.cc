/*
  Textdomain    "storage"
*/


#include "storage/AppUtil.h"
#include "storage/StorageTmpl.h"
#include "storage/OutputProcessor.h"


namespace storage
{
    using namespace std;


void
ProgressBar::setCurValue(unsigned val)
    {
    if( first || val!=cur )
	{
	y2mil("val:" << val);
	cur = val;
	first = false;
	if( callback )
	    (*callback)( id, cur, max );
	}
    }

void
Mke2fsProgressBar::process(const string& val, bool stderr)
    {
    y2deb("val:" << val << " err:" << stderr << " done:" << done);
    if( !stderr && !done )
	{
	seen += val;
	string::size_type pos;
	string::size_type bpos = seen.find( "\b" );
	y2deb("bpos:" << bpos);
	if( bpos==0 )
	    {
	    seen.erase( 0, seen.find_first_not_of( "\b", bpos ));
	    }
	while( first && (pos=seen.find( "\n" )) != string::npos && pos<bpos )
	    {
	    seen.erase( 0, pos+1 );
	    }
	while( bpos != string::npos )
	    {
	    pos = seen.find_first_of( "0123456789" );
	    if( pos != string::npos )
		seen.erase( 0, pos );
	    y2deb("seen:" << seen);
	    string number = seen.substr( 0, bpos );
	    y2deb("number:" << number);
	    list<string> l = splitString(number, "/" );
	    list<string>::const_iterator i = l.begin();
	    if( i != l.end() )
		{
		unsigned cval, mx;
		*i++ >> cval;
		if( first && i != l.end() )
		    {
		    *i >> mx;
		    setMaxValue( mx+4 );
		    setCurValue( 0 );
		    first = false;
		    }
		else if( !first )
		    {
		    setCurValue( cval );
		    }
		}
	    seen.erase( 0, seen.find_first_not_of( "\b", bpos ));
	    bpos = seen.find( "\b" );
	    }
	if( seen.find( "done" )!=string::npos )
	    {
	    setCurValue(getMaxValue() - 4);
	    done = true;
	    }
	}
    }

void
ReiserProgressBar::process(const string& val, bool stderr)
    {
    y2deb("val:" << val << " err:" << stderr);
    if( !stderr )
	{
	seen += val;
	string::size_type pos;
	string::size_type bpos = seen.find( "Initializing" );
	while( first && (pos=seen.find( "\n" )) != string::npos && pos<bpos )
	    {
	    seen.erase( 0, pos+1 );
	    }
	bpos = seen.find( "%" );
	while( bpos != string::npos )
	    {
	    pos = seen.find_first_of( "0123456789" );
	    if( pos != string::npos )
		seen.erase( 0, pos );
	    y2deb("seen:" << seen);
	    string number = seen.substr( 0, bpos );
	    y2deb("number:" << number);
	    unsigned cval;
	    number >> cval;
	    setCurValue( cval );
	    seen.erase( 0, seen.find_first_not_of( "%.", bpos ));
	    bpos = seen.find( "%" );
	    }
	}
    }

void
DasdfmtProgressBar::process(const string& val, bool stderr)
    {
    y2deb("val:" << val << " err:" << stderr);
    if( !stderr )
	{
	seen += val;
	string::size_type pos;
	string::size_type bpos = seen.find( '|' );
	while( first && (pos=seen.find( '\n' )) != string::npos && pos<bpos )
	    {
	    unsigned long cyl = 0;
	    seen >> cyl;
	    max_cyl += cyl;
	    y2mil( "cyl:" << cyl << " max_cyl:" << max_cyl);
	    seen.erase( 0, pos+1 );
	    }
	if( bpos != string::npos && max_cyl==0 )
	    {
	    y2err("max_cyl is zero, this should not happen");
	    max_cyl = 100;
	    }
	while( bpos != string::npos )
	    {
	    cur_cyl++;
	    setCurValue( cur_cyl*getMaxValue()/max_cyl );
	    seen.erase( 0, bpos+1 );
	    bpos = seen.find( "|" );
	    }
	}
    }

}
