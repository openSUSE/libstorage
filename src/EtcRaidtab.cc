// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/

#include <sstream>

#include "y2storage/AppUtil.h"
#include "y2storage/Regex.h"
#include "y2storage/EtcRaidtab.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/Md.h"

using namespace std;
using namespace storage;

EtcRaidtab::EtcRaidtab( const string& prefix ) 
    {
    mdadm_dev_line = -1;
    mdadmname = prefix+"/etc/mdadm.conf";
    mdadm = new AsciiFile( mdadmname ); 
    buildMdadmMap();
    }

EtcRaidtab::~EtcRaidtab()
    {
    delete mdadm;
    }

void 
EtcRaidtab::updateEntry( unsigned num, const list<string>& entries,
                         const string& mline, const list<string>& devs )
    {
    y2mil("num:" <<num);
    string dline;
    map<unsigned,entry>::iterator i = mtab.find( num );
    if( i != mtab.end() )
	{
	mdadm->replace( i->second.first, i->second.last-i->second.first+1, mline );
	}
    else
	{
	mdadm->append( mline );
	}
    if( mdadm_dev_line<0 )
	{
	dline = "DEVICE partitions";
	mdadm_dev_line = mdadm->numLines();
	mdadm->insert( 0, dline );
	}
    else
	{
	dline = "DEVICE partitions";
	(*mdadm)[mdadm_dev_line] = dline;
	}
    updateMdadmFile();
    }

void
EtcRaidtab::removeEntry( unsigned num )
    {
    map<unsigned,entry>::iterator i = mtab.find( num );
    if( i != mtab.end() )
	{
	mdadm->remove( i->second.first, i->second.last-i->second.first+1 );
	updateMdadmFile();
	}
    }

void
EtcRaidtab::updateMdadmFile()
    {
    mtab.clear();
    mdadm->updateFile();
    mdadm->loadFile( mdadmname );
    buildMdadmMap();
    }

void
EtcRaidtab::buildMdadmMap()
    {
    unsigned lineno = 0;
    unsigned mdnum;
    mdadm_dev_line = -1;
    entry e;
    while( lineno<mdadm->numLines() )
	{
	string key = extractNthWord( 0, (*mdadm)[lineno] );
	if( mdadm_dev_line<0 && key == "DEVICE"  )
	    mdadm_dev_line = lineno;
	else if( key == "ARRAY" &&
		 Md::mdStringNum( extractNthWord( 1, (*mdadm)[lineno] ), mdnum ))
	    {
	    e.first = lineno++;
	    while( lineno<mdadm->numLines() && 
	           (key = extractNthWord( 0, (*mdadm)[lineno] ))!="ARRAY" &&
		   key != "DEVICE" )
		{
		key = extractNthWord( 0, (*mdadm)[lineno++] );
		}
	    e.last = lineno-1;
	    mtab[mdnum] = e;
	    }
	else
	    lineno++;
	}
    }
