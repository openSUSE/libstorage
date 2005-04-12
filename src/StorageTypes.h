#ifndef STORAGE_TYPES_H
#define STORAGE_TYPES_H

#include <y2storage/Regex.h>
#include <y2storage/StorageInterface.h>

using namespace storage;

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

struct usedBy
    {
    usedBy() : t(UB_NONE) {;}
    usedBy( UsedByType typ, const string& n ) : t(typ), name(n) {;}
    void clear() { t=UB_NONE; name.erase(); }
    void set( UsedByType type, const string& n ) 
	{ t=type; (t==UB_NONE)?name.erase():name=n; }
    bool operator==( const usedBy& rhs ) const
	{ return( t==rhs.t && name==rhs.name ); }
    friend inline ostream& operator<< (ostream&, const usedBy& );

    UsedByType t;
    string name;
    };

inline ostream& operator<< (ostream& s, const usedBy& d )
    {
    if( d.t!=UB_NONE )
	{
	string st;
	switch( d.t )
	    {
	    case UB_LVM:
		st = "lvm";
		break;
	    case UB_MD: 
		st = "md";
		break;
	    case UB_EVMS: 
		st = "evms";
		break;
	    case UB_DM:
		st = "dm";
		break;
	    default:
		st = "UNKNOWN";
		break;
	    }
	s << " UsedBy:" << st << "[" << d.name << "]";
	}
    return( s );
    }

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
