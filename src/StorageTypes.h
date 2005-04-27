#ifndef STORAGE_TYPES_H
#define STORAGE_TYPES_H

#include <ostream>

#include "y2storage/Regex.h"
#include "y2storage/StorageInterface.h"

using std::ostream;

typedef enum { CUNKNOWN, DISK, MD, LOOP, LVM, DM, EVMS } CType;

struct contOrder
    {
    contOrder(CType t) : order(0)
	{
	if( t==LOOP )
	    order=1;
	}
    operator unsigned() const { return( order ); }
    protected:
	unsigned order;
    };

typedef enum { DECREASE, INCREASE, FORMAT, MOUNT } CommitStage;

struct commitAction
    {
    commitAction( CommitStage s, CType t, const string& d, bool destr=false,
                  bool cont=false ) 
	{ stage=s; type=t; descr=d; destructive=destr; container=cont; }
    CommitStage stage;
    CType type;
    string descr;
    bool destructive;
    bool container;
    bool operator==( const commitAction& rhs ) const
	{ return( stage==rhs.stage && type==rhs.type ); }
    bool operator<( const commitAction& rhs ) const
	{
	contOrder l(type);
	contOrder r(rhs.type);

	if( unsigned(r)==unsigned(l) )
	    {
	    if( stage==rhs.stage )
		{
		if( stage==DECREASE )
		    {
		    if( type!=rhs.type )
			return( type>rhs.type );
		    else
			return( container<rhs.container );
		    }
		else
		    {
		    if( type!=rhs.type )
			return( type<rhs.type );
		    else
			return( container>rhs.container );
		    }
		}
	    else
		return( stage<rhs.stage );
	    }
	else
	    return( unsigned(l)<unsigned(r) );
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
    usedBy() : t(storage::UB_NONE) {;}
    usedBy( storage::UsedByType typ, const string& n ) : t(typ), name(n) {;}
    void clear() { t=storage::UB_NONE; name.erase(); }
    void set( storage::UsedByType type, const string& n ) 
	{ t=type; (t==storage::UB_NONE)?name.erase():name=n; }
    bool operator==( const usedBy& rhs ) const
	{ return( t==rhs.t && name==rhs.name ); }
    friend inline ostream& operator<< (ostream&, const usedBy& );

    storage::UsedByType t;
    string name;
    };

inline ostream& operator<< (ostream& s, const usedBy& d )
    {
    if( d.t!=storage::UB_NONE )
	{
	string st;
	switch( d.t )
	    {
	    case storage::UB_LVM:
		st = "lvm";
		break;
	    case storage::UB_MD: 
		st = "md";
		break;
	    case storage::UB_EVMS: 
		st = "evms";
		break;
	    case storage::UB_DM:
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
