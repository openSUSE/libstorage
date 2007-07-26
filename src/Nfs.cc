/*
  Textdomain    "storage"
*/

#include <sstream>

#include <sys/stat.h>

#include "y2storage/Nfs.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/Container.h"
#include "y2storage/AppUtil.h"
#include "y2storage/Storage.h"
#include "y2storage/SystemCmd.h"

using namespace storage;
using namespace std;

Nfs::Nfs( const NfsCo& d, const string& NfsDev ) :
    Volume( d, 0, 0 )
    {
    y2debug( "constructed nfs dev:%s", NfsDev.c_str() );
    if( d.type() != NFSC )
	y2error( "constructed nfs with wrong container" );
    dev = canonicalName(NfsDev);
    if( dev != NfsDev )
	alt_names.push_back( NfsDev );
    init();
    }

Nfs::~Nfs()
    {
    y2debug( "destructed nfs %s", dev.c_str() );
    }

string Nfs::removeText( bool doing ) const
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by volume name e.g. hilbert:/work
	txt = sformat( _("Removing nfs volume %1$s"), dev.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by volume name e.g. hilbert:/work
	txt = sformat( _("Remove nfs volume %1$s"), dev.c_str() );
	}
    return( txt );
    }

void
Nfs::init()
    {
    numeric = false;
    nm = dev;
    setFs(NFS);
    }

string Nfs::canonicalName( const string& d )
    {
    string dev(d);
    string::size_type pos = 0;
    while( (pos=dev.find("//",pos))!=string::npos )
	dev.erase(pos,1);
    if( !dev.empty() && *dev.rbegin()=='/' )
	dev.erase(dev.size()-1);
    if( dev!=d )
	y2mil( "dev:" << dev << " d:" << d );
    return(dev);
    }

void Nfs::getInfo( NfsInfo& tinfo ) const
    {
    ((Volume*)this)->getInfo( info.v );
    tinfo = info;
    }

namespace storage
{

std::ostream& operator<< (std::ostream& s, const Nfs& l )
    {
    s << "Nfs " << *(Volume*)&l;
    return( s );
    }

}

bool Nfs::equalContent( const Nfs& rhs ) const
    {
    return( Volume::equalContent(rhs) );
    }

void Nfs::logDifference( const Nfs& rhs ) const
    {
    string log = Volume::logDifference( rhs );
    y2milestone( "%s", log.c_str() );
    }

Nfs& Nfs::operator= ( const Nfs& rhs )
    {
    y2debug( "operator= from %s", rhs.nm.c_str() );
    *((Volume*)this) = rhs;
    return( *this );
    }

Nfs::Nfs( const NfsCo& d, const Nfs& rhs ) : Volume(d)
    {
    y2debug( "constructed nfs by copy constructor from %s", rhs.nm.c_str() );
    *this = rhs;
    }

