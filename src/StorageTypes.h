#ifndef STORAGE_TYPES_H
#define STORAGE_TYPES_H

#include <iostream>

#include "y2storage/Regex.h"
#include "y2storage/AppUtil.h"
#include "y2storage/StorageInterface.h"

namespace storage
{

inline bool operator<(CType a, CType b)
{
    static const int order[COTYPE_LAST_ENTRY] = {
	0, // CUNKNOWN
	1, // DISK
	3, // MD
	7, // LOOP
	5, // LVM
	4, // DM
	6, // EVMS
	2, // DMRAID
	8  // NFSC
    };

    bool ret = order[a] < order[b];
    y2mil("a:" << a << " o(a):" << order[a] << " b:" << b << " o(b):" << order[b] << " ret:" << ret);
    return ret;
}

inline bool operator<=( CType a, CType b )
    {
    return( a==b || a<b );
    }

inline bool operator>=( CType a, CType b )
    {
    return( !(a<b) );
    }

inline bool operator>( CType a, CType b )
    {
    return( a!=b && !(a<b) );
    }

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

class Volume;
class Container;

struct commitAction
    {
    commitAction( CommitStage s, CType t, const string& d, const Volume* v,
                  bool destr=false )
	{ stage=s; type=t; descr=d; destructive=destr; container=false;
	  u.vol=v; }
    commitAction( CommitStage s, CType t, const string& d, const Container* co,
                  bool destr=false )
	{ stage=s; type=t; descr=d; destructive=destr; container=true;
	  u.co=co; }
    commitAction( CommitStage s, CType t, Volume* v )
	{ stage=s; type=t; destructive=false; container=false; u.vol=v; }
    commitAction( CommitStage s, CType t, Container* c )
	{ stage=s; type=t; destructive=false; container=true; u.co=c; }
    CommitStage stage;
    CType type;
    string descr;
    bool destructive;
    bool container;
    union
	{
	const Volume* vol;
	const Container* co;
	} u;
    const Container* co() const { return( container?u.co:NULL ); }
    const Volume* vol() const { return( container?NULL:u.vol ); }
    bool operator==( const commitAction& rhs ) const
	{ return( stage==rhs.stage && type==rhs.type ); }
    bool operator<( const commitAction& rhs ) const;
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
    usedBy( storage::UsedByType typ, const string& n ) : t(typ), nm(n) {;}
    void clear() { t=storage::UB_NONE; nm.erase(); }
    void set( storage::UsedByType type, const string& n )
	{ t=type; (t==storage::UB_NONE)?nm.erase():nm=n; }
    bool operator==( const usedBy& rhs ) const
	{ return( t==rhs.t && nm==rhs.nm ); }
    bool operator!=( const usedBy& rhs ) const
	{ return( !(*this==rhs)); }
    inline operator string() const;

    storage::UsedByType type() const { return( t ); }
    const string& name() const { return( nm ); }
    friend inline std::ostream& operator<< (std::ostream&, const usedBy& );

    storage::UsedByType t;
    string nm;
    };

inline usedBy::operator string() const
    {
    string st;
    if( t!=storage::UB_NONE )
	{
	switch( t )
	    {
	    case storage::UB_LVM:
		st = "lvm";
		break;
	    case storage::UB_MD:
		st = "md";
		break;
	    case storage::UB_DM:
		st = "dm";
		break;
	    case storage::UB_DMRAID:
		st = "dmraid";
		break;
	    default:
		st = "UNKNOWN";
		break;
	    }
	st += "[" + nm + "]";
	}
    return( st );
    }

inline std::ostream& operator<< (std::ostream& s, const usedBy& d )
    {
    if( d.t!=storage::UB_NONE )
	{
	s << " UsedBy:" << string(d);
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

struct find_any
    {
    find_any( const string& t ) : val(t) {};
    bool operator()(const string&s) { return( s.find(val)!=string::npos ); }
    const string& val;
    };

}

#endif
