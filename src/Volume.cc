#include <sstream>

#include <features.h>
#include <sys/stat.h>
#include <ycp/y2log.h>

#include "y2storage/Volume.h"
#include "y2storage/Disk.h"
#include "y2storage/Container.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"

Volume::Volume( const Container& d, unsigned PNr, unsigned long long SizeK ) 
    : cont(&d)
    {
    numeric = true;
    num = PNr;
    size_k = SizeK;
    init();
    y2milestone( "constructed volume %s on disk %s", (num>0)?dev.c_str():"",
                 cont->name().c_str() );
    }

Volume::Volume( const Container& c, const string& Name, unsigned long long SizeK ) : cont(&c)
    {
    numeric = false;
    nm = Name;
    size_k = SizeK;
    init();
    y2milestone( "constructed volume \"%s\" on disk %s", dev.c_str(),
                 cont->name().c_str() );
    }

Volume::~Volume()
    {
    y2milestone( "destructed volume %s", dev.c_str() );
    }

void Volume::setNameDev()
    {
    std::ostringstream Buf_Ci;
    if( numeric )
	Buf_Ci << cont->device() << (Disk::needP(cont->device())?"p":"") << num;
    else
	Buf_Ci << cont->device() << "/" << nm;
    dev = Buf_Ci.str();
    nm = dev.substr( 5 );
    }

void Volume::init()
    {
    del = create = format = false;
    detected_fs = fs = UNKNOWN;
    mount_by = orig_mount_by = MOUNTBY_DEVICE;
    setNameDev();
    mjr = mnr = 0;
    getMajorMinor( dev, mjr, mnr );
    if( !numeric )
	num = 0;
    }

bool Volume::operator== ( const Volume& rhs ) const
    {
    return( (*cont)==(*rhs.cont) && 
            nm == rhs.nm && 
	    del == rhs.del ); 
    }

bool Volume::operator< ( const Volume& rhs ) const
    {
    if( *cont != *rhs.cont )
	return( *cont<*rhs.cont );
    else if( nm != rhs.nm )
	{
	if( numeric )
	    return( num<rhs.num );
	else
	    return( nm<rhs.nm );
	}
    else
	return( !del );
    }

bool Volume::getMajorMinor( const string& device,
			    unsigned long& Major, unsigned long& Minor )
    {
    bool ret = false;
    string dev( device );
    if( dev.find( "/dev/" )!=0 )
	dev = "/dev/"+ dev;
    struct stat sbuf;
    if( stat( dev.c_str(), &sbuf )==0 )
	{
	Minor = gnu_dev_minor( sbuf.st_rdev );
	Major = gnu_dev_major( sbuf.st_rdev );
	ret = true;
	}
    return( ret );
    }

void Volume::getFsData( SystemCmd& blkidData )
    {
    bool found = blkidData.select( "^" + device() + ":" )>0;
    if( !found )
	{
	list<string>::const_iterator an = alt_names.begin();
	while( !found && an!=alt_names.end() )
	    {
	    found = blkidData.select( "^" + *an + ":" )>0;
	    ++an;
	    }
	}
    if( found )
	{
	list<string> l = splitString( *blkidData.getLine( 0, true ));
	std::ostringstream b;
	b << "line[" << device() << "]=" << l;
	y2milestone( "%s", b.str().c_str() );
	if( l.size()>0 )
	    {
	    l.pop_front();
	    map<string,string> m = makeMap( l, "=", "\"" );
	    b.str("");
	    if( m.find( "TYPE" )!=m.end() )
		{
		if( m["TYPE"] == "reiserfs" )
		    {
		    fs = REISERFS;
		    }
		else if( m["TYPE"] == "swap" )
		    {
		    fs = SWAP;
		    }
		else if( m["TYPE"] == "ext2" )
		    {
		    fs = (m["SEC_TYPE"]=="ext3")?EXT3:EXT2;
		    }
		else if( m["TYPE"] == "vfat" )
		    {
		    fs = VFAT;
		    }
		else if( m["TYPE"] == "ntfs" )
		    {
		    fs = NTFS;
		    }
		else if( m["TYPE"] == "jfs" )
		    {
		    fs = JFS;
		    }
		else if( m["TYPE"] == "xfs" )
		    {
		    fs = XFS;
		    }
		detected_fs = fs;
		b << "fs:" << fs_names[fs];
		}
	    if( m.find("UUID") != m.end() )
		{
		uuid = m["UUID"];
		b << " uuid:" << uuid;
		}
	    if( m.find("LABEL") != m.end() )
		{
		label = orig_label = m["LABEL"];
		b << " label:\"" << label << "\"";
		}
	    y2milestone( "%s", b.str().c_str() );
	    }
	}
    }


string Volume::fs_names[] = { "unknown", "reiser", "ext2", "ext2", "vfat",
                              "xfs", "jfs", "ntfs", "swap" };

string Volume::mb_names[] = { "device", "uuid", "label" };



