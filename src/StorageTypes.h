#ifndef STORAGE_TYPES_H
#define STORAGE_TYPES_H

#include <y2storage/Regex.h>

typedef enum { CUNKNOWN, DISK, MD, LOOP, LVM, EVMS } CType;

typedef enum { DECREASE, INCREASE, FORMAT, MOUNT } CommitStage;

struct commitAction
    {
    commitAction( CommitStage s, CType t, const string& d, bool destr=false ) 
	{ stage=s; type=t; descr=d; destructive=destr; }
    CommitStage stage;
    CType type;
    string descr;
    bool destructive;
    bool operator==( const commitAction& rhs ) const
	{ return( stage==rhs.stage && type==rhs.type ); }
    bool operator<( const commitAction& rhs ) const
	{ 
	if( stage==rhs.stage )
	    {
	    if( stage==DECREASE )
		return( type>rhs.type );
	    else
		return( type<rhs.type );
	    }
	else
	    return( stage<rhs.stage );
	}
    bool operator<=( const commitAction& rhs ) const
	{ return( *this < rhs || *this == rhs ); }
    bool operator>=( const commitAction& rhs ) const
	{ return( ! (*this < rhs) ); }
    bool operator>( const commitAction& rhs ) const
	{ return( !(*this < rhs && *this == rhs) ); }
    };

struct match_string
    {
    match_string( const string& t ) : r(t) {};
    bool operator()(const string&s) { return( r.match( s )); }
    Regex r;
    };

struct find_begin
    {
    find_begin( const string& t ) : val(t) {};
    bool operator()(const string&s) { return( s.find(val)==0 ); }
    const string& val;
    };

#endif
