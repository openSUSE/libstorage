#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/AppUtil.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/OutputProcessor.h"

void
OutputProcessor::process( const string& val, bool stderr )
    {
    y2milestone( "stderr:%d val:%s", stderr, val.c_str() );
    }

void
ScrollBarHandler::process( const string& val, bool stderr )
    {
    OutputProcessor::process( val, stderr );
    static int cnt=0;
    setCurValue( cnt++/2 );
    }

void
ScrollBarHandler::setCurValue( unsigned val )
    {
    if( first || val!=cur )
	{
	y2milestone( "val %u", val );
	cur = val;
	first = false;
	if( callback )
	    (*callback)( id, cur, max );
	}
    }

void
Mke2fsScrollbar::process( const string& val, bool stderr )
    {
    y2debug( "val:%s err:%d done:%d", val.c_str(), stderr, done );
    if( !stderr && !done )
	{
	seen += val;
	string::size_type pos;
	string::size_type bpos = seen.find( "\b" );
	y2debug( "bpos:%u", bpos );
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
	    y2debug( "seen:%s", seen.c_str() );
	    string number = seen.substr( 0, bpos );
	    y2debug( "number:%s", seen.c_str() );
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
	    setCurValue( max-4 );
	    done = true;
	    }
	}
    }

void
ReiserScrollbar::process( const string& val, bool stderr )
    {
    y2debug( "val:%s err:%d", val.c_str(), stderr );
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
	    y2debug( "seen:%s", seen.c_str() );
	    string number = seen.substr( 0, bpos );
	    unsigned cval;
	    number >> cval;
	    setCurValue( cval );
	    seen.erase( 0, seen.find_first_not_of( "%.", bpos ));
	    bpos = seen.find( "%" );
	    }
	}
    }
