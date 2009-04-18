
#include "y2storage/Storage.h"
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


    std::ostream& operator<<(std::ostream& s, const commitAction& a)
    {
	s << "stage:" << a.stage
	  << " type:" << a.type
	  << " cont:" << a.container
	  << " dest:" << a.destructive;
	if (a.co())
	    s << " name:" << a.co()->name();
	if (a.vol())
	    s << " name:" << a.vol()->name();
	if (!a.description.empty())
	    s << " desc:" << a.description;
	return s;
    };


    const string usedBy::device() const
    {
	switch (ub_type)
	{
	    case UB_NONE:
		return "";

	    case UB_LVM:
	    case UB_MD:
		return "/dev/" + ub_name;

	    case UB_DM:
	    case UB_DMRAID:
	    case UB_DMMULTIPATH:
		return "/dev/mapper/" + ub_name;
	}
    }


    usedBy::operator string() const
    {
	string st;
	if (type() != storage::UB_NONE)
	{
	    switch (type())
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
		case UB_DMMULTIPATH:
		    st = "dmmultipath";
		    break;
		default:
		    st = "UNKNOWN";
		    break;
	    }
	    st += "[" + device() + "]";
	}
	return st;
    }


    std::ostream& operator<<(std::ostream& s, const usedBy& d)
    {
	if (d.type() != storage::UB_NONE)
	    s << string(d);
	return s;
    }

}
