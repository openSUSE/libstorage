#ifndef STORAGE_TYPES_H
#define STORAGE_TYPES_H

#include <ostream>
#include <boost/algorithm/string.hpp>

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
	4, // MD
	7, // LOOP
	6, // LVM
	5, // DM
	2, // DMRAID
	8, // NFSC
	3  // DMMULTIPATH
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
    commitAction( CommitStage s, CType t, const Volume* v )
	{ stage=s; type=t; destructive=false; container=false; u.vol=v; }
    commitAction( CommitStage s, CType t, const Container* c )
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


class usedBy
{
    // TODO: save device instead of name?

public:
    usedBy() : ub_type(storage::UB_NONE) {}
    usedBy(storage::UsedByType type, const string& name) : ub_type(type), ub_name(name) {}

    void clear() { ub_type = storage::UB_NONE; ub_name.erase(); }
    void set(storage::UsedByType type, const string& name)
	{ ub_type = type; (ub_type==storage::UB_NONE)?ub_name.erase():ub_name = name; }

    bool operator==(const usedBy& rhs) const
	{ return ub_type == rhs.ub_type && ub_name == rhs.ub_name; }
    bool operator!=(const usedBy& rhs) const
	{ return !(*this == rhs); }

    operator string() const;

    storage::UsedByType type() const { return ub_type; }
    const string& name() const { return ub_name; }
    const string device() const;

    friend std::ostream& operator<<(std::ostream&, const usedBy&);

private:
    storage::UsedByType ub_type;
    string ub_name;
};


    struct regex_matches
    {
	regex_matches(const Regex& t) : val(t) {}
	bool operator()(const string& s) { return val.match(s); }
	const Regex& val;
    };

    struct string_starts_with
    {
	string_starts_with(const string& t) : val(t) {}
	bool operator()(const string& s) { return boost::starts_with(s, val); }
	const string& val;
    };

    struct string_contains
    {
	string_contains(const string& t) : val(t) {}
	bool operator()(const string& s) { return boost::contains(s, val); }
	const string& val;
    };

}

#endif
