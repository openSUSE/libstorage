// Maintainer: fehr@suse.de

#include <ycp/y2log.h>

#include <sstream>
#include <algorithm>

#include "y2storage/AppUtil.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/Volume.h"
#include "y2storage/EtcFstab.h"

using namespace storage;

struct find_begin
    {
    find_begin( const string& t ) : val(t) {};
    bool operator()(const string&s) { return( s.find(val)==0 ); }
    const string& val;
    };

EtcFstab::EtcFstab( const string& prefix )
    {
    sync = true;
    string file = prefix+"/etc/fstab";
    ifstream mounts( file.c_str() );
    string line;
    getline( mounts, line );
    while( mounts.good() )
	{
	list<string> l = splitString( line );
	list<string>::const_iterator i = l.begin();
	Entry *p = new Entry;

	if( i!=l.end() )
	    p->old.device = *i++;
	if( i!=l.end() )
	    p->old.mount = *i++;
	if( i!=l.end() )
	    p->old.fs = *i++;
	if( i!=l.end() )
	    p->old.opts = splitString( *i++, "," );
	if( i!=l.end() )
	    *i++ >> p->old.freq;
	if( i!=l.end() )
	    *i++ >> p->old.passno;

	list<string>::const_iterator beg = p->old.opts.begin();
	list<string>::const_iterator end = p->old.opts.end();

	p->old.noauto = find( beg, end, "noauto" ) != end;

	i = find_if( beg, end, *new find_begin("loop") );
	if( i!=end )
	    {
	    p->old.loop = true;
	    string::size_type pos = i->find("=");
	    if( pos!=string::npos )
		{
		p->old.loop_dev = i->substr( pos+1 );
		}
	    }
	i = find_if( beg, end, *new find_begin("encryption=") );
	if( i!=end )
	    {
	    string::size_type pos = i->find("=");
	    if( pos!=string::npos )
		{
		p->old.encr = Volume::toEncType( i->substr( pos+1 ) );
		}
	    }
	if( p->old.device.find( "LABEL=" )==0 )
	    p->old.mount_by = MOUNTBY_LABEL;
	else if( p->old.device.find( "UUID=" )==0 )
	    p->old.mount_by = MOUNTBY_UUID;
	p->nnew = p->old;
	co.push_back( *p );
	delete p;

	getline( mounts, line );
	}
    mounts.close();
    file = prefix+"/etc/cryptotab";
    mounts.clear();
    mounts.open( file.c_str() );
    getline( mounts, line );
    while( mounts.good() )
	{
	list<string> l = splitString( line );
	list<string>::const_iterator i = l.begin();
	Entry *p = new Entry;

	p->old.loop = p->old.crypto = true;
	if( i!=l.end() )
	    p->old.loop_dev = *i++;
	if( i!=l.end() )
	    p->old.device = *i++;
	if( i!=l.end() )
	    p->old.mount = *i++;
	if( i!=l.end() )
	    p->old.fs = *i++;
	if( i!=l.end() )
	    p->old.encr = Volume::toEncType( *i++ );
	if( i!=l.end() )
	    p->old.opts = splitString( *i++, "," );
	p->nnew = p->old;
	co.push_back( *p );
	delete p;

	getline( mounts, line );
	}
    }

bool 
EtcFstab::findDevice( const string& dev, FstabEntry& entry ) const
    {
    list<Entry>::const_iterator i = co.begin();
    while( i!=co.end() && i->nnew.device != dev )
	++i;
    if( i!=co.end() )
	entry = i->nnew;
    return( i!=co.end() );
    }

bool 
EtcFstab::findDevice( const list<string>& dl, FstabEntry& entry ) const
    {
    list<Entry>::const_iterator i = co.begin();
    while( i!=co.end() && 
           find( dl.begin(), dl.end(), i->nnew.device )==dl.end() )
	++i;
    if( i!=co.end() )
	entry = i->nnew;
    return( i!=co.end() );
    }

bool 
EtcFstab::findMount( const string& mount, FstabEntry& entry ) const
    {
    list<Entry>::const_iterator i = co.begin();
    while( i!=co.end() && i->nnew.mount != mount )
	++i;
    if( i!=co.end() )
	entry = i->nnew;
    return( i!=co.end() );
    }

bool 
EtcFstab::findUuidLabel( const string& uuid, const string& label, 
                         FstabEntry& entry ) const
    {
    y2milestone( "uuid:%s label:%s", uuid.c_str(), label.c_str() );
    list<Entry>::const_iterator i = co.begin();
    if( uuid.size()>0 )
	{
	string dev = "UUID=" + uuid;
	while( i!=co.end() && i->nnew.device != dev )
	    ++i;
	}
    if( i==co.end() && label.size()>0 )
	{
	string dev = "LABEL=" + label;
	i = co.begin();
	while( i!=co.end() && i->nnew.device != dev )
	    {
	    y2milestone( "dev:%s dev:%s", dev.c_str(), i->nnew.device.c_str() );
	    ++i;
	    }
	}
    if( i!=co.end())
	entry = i->nnew;
    y2milestone( "ret:%d", i!=co.end() );
    return( i!=co.end() );
    }

int EtcFstab::removeEntry( const FstabEntry& entry )
    {
    list<Entry>::iterator i = co.begin();
    while( i->op==Entry::REMOVE || (i->old != entry && i->nnew != entry) )
	++i;
    if( i != co.end() )
	{
	if( i->op != Entry::ADD )
	    i->op = Entry::REMOVE;
	else
	    co.erase( i );
	}
    return( (i != co.end())?0:FSTAB_ENTRY_NOT_FOUND );
    }

