// Maintainer: fehr@suse.de

#include <ycp/y2log.h>

#include <sstream>

#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/EtsFstab.h"

EtcFstab::EtcFstab() 
    {
    sync = true;
    ifstream mounts( "/etc/fstab" );
    string line;
    getline( mounts, line );
    while( mounts.good() )
	{
	string dev = extractNthWord( 0, line );
	string mount = extractNthWord( 1, line );
	getline( mounts, line );
	}
    mounts.close();
    mounts.open( "/etc/cryptotab" );
    getline( mounts, line );
    while( mounts.good() )
	{
	string dev = extractNthWord( 0, line );
	getline( mounts, line );
	}
    }



