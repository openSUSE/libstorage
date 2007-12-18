#include "y2storage/StorageTypes.h"
#include "y2storage/Volume.h"

namespace storage
{

bool commitAction::operator<( const commitAction& rhs ) const
    {
    contOrder l(type);
    contOrder r(rhs.type);

    if( stage==rhs.stage && stage==MOUNT )
	{
	if( vol()==NULL || rhs.vol()==NULL )
	    return( false );
	else 
	    {
	    if( rhs.vol()->getMount()=="swap" )
		return( false );
	    else if( vol()->getMount()=="swap" )
		return( true );
	    else if( vol()->hasOrigMount() != rhs.vol()->hasOrigMount() )
		return( rhs.vol()->hasOrigMount() );
	    else
		return( vol()->getMount()<rhs.vol()->getMount() );
	    }
	}
    else if( unsigned(r)==unsigned(l) )
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

}

