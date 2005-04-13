#include <sstream>

#include <ycp/y2log.h>

#include "y2storage/LvmLv.h"
#include "y2storage/LvmVg.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"

LvmLv::LvmLv( const LvmVg& d, const string& name, unsigned long le,
	      const string& uuid, const string& stat, const string& alloc ) :
	Volume( d, name, 0 )
    {
    init();
    setUuid( uuid );
    setStatus( stat );
    setAlloc( alloc );
    setLe( le );
    getTableInfo();
    if( majorNr()>0 )
	{
	alt_names.push_back( "/dev/dm-" + decString(minorNr()) );
	}
    alt_names.push_back( "/dev/mapper/" + cont->name() + "-" + name );
    y2milestone( "constructed lvm lv %s on vg %s", dev.c_str(),
                 cont->name().c_str() );
    }

LvmLv::~LvmLv()
    {
    y2milestone( "destructed lvm lv %s", dev.c_str() );
    }

void
LvmLv::getTableInfo()
    {
    SystemCmd c( "dmsetup table " + cont->name() + "-" + name() );
    if( c.numLines()>0 )
	{
	string line = *c.getLine(0);
	if( extractNthWord( 2, line )=="striped" )
	    extractNthWord( 3, line ) >> stripe;
	}
    pe_map.clear();
    map<string,unsigned long>::iterator mit;
    unsigned long long pesize = vg()->peSize();
    for( unsigned i=0; i<c.numLines(); i++ )
	{
	unsigned long le;
	string dev;
	string majmin;
	string line = *c.getLine(i);
	string type = extractNthWord( 2, line );
	if( type=="linear" )
	    {
	    extractNthWord( 1, line ) >> le;
	    le *=2;
	    le += pesize-1;
	    le /= pesize;
	    majmin = extractNthWord( 3, line );
	    dev = cont->getStorage()->deviceByNumber( majmin );
	    if( dev.size()>0 )
		{
		if( (mit=pe_map.find( dev ))==pe_map.end() )
		    pe_map[dev] = le;
		else
		    mit->second += le;
		}
	    else
		y2warning( "could not find major/minor pair %s", 
		           majmin.c_str());
	    }
	else if( type=="striped" )
	    {
	    unsigned str;
	    extractNthWord( 1, line ) >> le;
	    extractNthWord( 3, line ) >> str;
	    le *=2;
	    le += pesize-1;
	    le /= pesize;
	    if( str<2 )
		y2warning( "invalid stripe count %u", str );
	    else
		{
		le = (le+str-1)/str;
		for( unsigned j=0; j<str; i++ )
		    {
		    majmin = extractNthWord( 5+j*str*2, line );
		    dev = cont->getStorage()->deviceByNumber( majmin );
		    if( dev.size()>0 )
			{
			if( (mit=pe_map.find( dev ))==pe_map.end() )
			    pe_map[dev] = le;
			else
			    mit->second += le;
			}
		    else
			y2warning( "could not find major/minor pair %s", 
				   majmin.c_str());
		    }
		}
	    }
	else
	    {
	    y2warning( "unknown table type \"%s\"", type.c_str() );
	    }
	}
    }

void
LvmLv::setLe( unsigned long le )
    {
    num_le = le;
    calcSize();
    }

void LvmLv::init()
    {
    num_le = 0;
    stripe = 1;
    }

const LvmVg* const LvmLv::vg() const
    { 
    return(dynamic_cast<const LvmVg* const>(cont));
    }
	        
void LvmLv::calcSize()
    {
    setSize( num_le*vg()->peSize() );
    }

