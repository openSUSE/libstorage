// Maintainer: fehr@suse.de

#include <ycp/y2log.h>

#include <sstream>
#include <algorithm>

#include "y2storage/AppUtil.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/Regex.h"
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
	    p->old.device = p->old.dentry = *i++;
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
	    {
	    p->old.mount_by = MOUNTBY_LABEL;
	    p->old.device.erase();
	    }
	else if( p->old.device.find( "UUID=" )==0 )
	    {
	    p->old.mount_by = MOUNTBY_UUID;
	    p->old.device.erase();
	    }
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
	    p->old.device = p->old.dentry = *i++;
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

void
FstabEntry::calcDependent()
    {
    list<string>::const_iterator beg = opts.begin();
    list<string>::const_iterator end = opts.end();

    noauto = find( beg, end, "noauto" ) != end;

    list<string>::const_iterator i = find_if( beg, end, *new find_begin("loop") );
    if( i!=end )
	{
	loop = true;
	string::size_type pos = i->find("=");
	if( pos!=string::npos )
	    {
	    loop_dev = i->substr( pos+1 );
	    }
	}
    i = find_if( beg, end, *new find_begin("encryption=") );
    if( i!=end )
	{
	string::size_type pos = i->find("=");
	if( pos!=string::npos )
	    {
	    encr = Volume::toEncType( i->substr( pos+1 ) );
	    }
	}
    if( device.find( "LABEL=" )==0 )
	{
	mount_by = MOUNTBY_LABEL;
	device.erase();
	}
    else if( device.find( "UUID=" )==0 )
	{
	mount_by = MOUNTBY_UUID;
	device.erase();
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

int EtcFstab::changeRootPrefix( const string& prfix )
    {
    int ret = 0;
    if( prfix != prefix )
	{
	list<Entry>::const_iterator i = co.begin();
	while( i!=co.end() && i->op == Entry::ADD )
	    ++i;
	if( i==co.end() )
	    prefix = prfix;
	else 
	    ret = FSTAB_CHANGE_PREFIX_IMPOSSIBLE;
	}
    y2milestone( "new prefix:%s ret:%d", prefix.c_str(), ret );
    return( ret );
    }

void EtcFstab::setDevice( const FstabEntry& entry, const string& device )
    {
    list<Entry>::iterator i = co.begin();
    while( i!=co.end() && i->old != entry )
	++i;
    if( i!=co.end() )
	i->nnew.device = i->old.device = device;
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
	while( i!=co.end() && i->nnew.dentry != dev )
	    ++i;
	}
    if( i==co.end() && label.size()>0 )
	{
	string dev = "LABEL=" + label;
	i = co.begin();
	while( i!=co.end() && i->nnew.dentry != dev )
	    ++i;
	}
    if( i!=co.end())
	entry = i->nnew;
    y2milestone( "ret:%d", i!=co.end() );
    return( i!=co.end() );
    }

int EtcFstab::removeEntry( const FstabEntry& entry )
    {
    list<Entry>::iterator i = co.begin();
    while( i->op==Entry::REMOVE || i->nnew != entry )
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

int EtcFstab::updateEntry( const FstabChange& entry )
    {
    list<Entry>::iterator i = co.begin();
    while( i->op==Entry::REMOVE || (i->nnew != entry && i->old != entry) )
	++i;
    if( i != co.end() )
	{
	if( i->op==Entry::NONE )
	    i->op = Entry::UPDATE;
	i->nnew = entry;
	}
    return( (i != co.end())?0:FSTAB_ENTRY_NOT_FOUND );
    }

int EtcFstab::addEntry( const FstabChange& entry )
    {
    Entry e;
    e.op = Entry::ADD;
    e.nnew = entry;
    co.push_back( e );
    return( 0 );
    }

AsciiFile* EtcFstab::findFile( const FstabEntry& e, AsciiFile*& fstab,
                               AsciiFile*& cryptotab, int& lineno )
    {
    AsciiFile* ret=NULL;
    Regex *fi = NULL;
    if( e.crypto )
	{
	if( cryptotab==NULL )
	    cryptotab = new AsciiFile( prefix + "/etc/cryptotab" );
	ret = cryptotab;
	fi = new Regex( "[ \t]" + e.dentry + "[ \t]" );
	}
    else
	{
	if( fstab==NULL )
	    fstab = new AsciiFile( prefix + "/etc/fstab" );
	ret = fstab;
	fi = new Regex( "^[ \t]*" + e.dentry + "[ \t]" );
	}
    lineno = ret->find( 0, *fi );
    delete fi;
    return( ret );
    }

void EtcFstab::makeStringList( const FstabEntry& e, list<string>& ls )
    {
    ls.clear();
    if( e.crypto )
	{
	ls.push_back( e.loop_dev );
	}
    ls.push_back( e.dentry );
    ls.push_back( e.mount );
    ls.push_back( e.fs );
    if( e.crypto )
	{
	ls.push_back( Volume::encTypeString(e.encr) );
	}
    ls.push_back( mergeString( e.opts, "," ) );
    if( !e.crypto )
	{
	ls.push_back( decString(e.freq) );
	ls.push_back( decString(e.passno) );
	}
    }

string EtcFstab::createTabLine( const FstabEntry& e )
    {
    string ret;
    list<string> ls;
    makeStringList( e, ls );
    int count=0;
    int max_fields = e.crypto ? sizeof(cryptotabFields)/sizeof(cryptotabFields[0])
			      : sizeof(fstabFields)/sizeof(fstabFields[0]);
    unsigned * fields = e.crypto ? cryptotabFields : fstabFields;
    for( list<string>::const_iterator i=ls.begin(); i!=ls.end(); ++i )
	{
	if( i != ls.begin() )
	    ret += " ";
	ret += *i;
	if( count<max_fields && i->size()<fields[count] )
	    ret.replace( string::npos, 0, fields[count]-i->size(), ' ' );
	count++;
	}
    return( ret );
    }

int EtcFstab::flush()
    {
    int ret = 0;
    list<Entry>::iterator i = co.begin();
    AsciiFile *fstab = NULL;
    AsciiFile *cryptotab = NULL;
    AsciiFile *cur = NULL;
    int lineno;
    while( i!=co.end() && ret==0 )
	{
	switch( i->op )
	    {
	    case Entry::REMOVE:
		cur = findFile( i->old, fstab, cryptotab, lineno );
		if( lineno>=0 )
		    cur->remove( lineno, 1 );
		else
		    ret = FSTAB_REMOVE_ENTRY_NOT_FOUND;
		i = co.erase( i );
		break;
	    case Entry::UPDATE:
		cur = findFile( i->old, fstab, cryptotab, lineno );
		if( lineno>=0 )
		    {
		    string line;
		    if( i->old.crypto != i->nnew.crypto )
			{
			cur->remove( lineno, 1 );
			cur = findFile( i->nnew, fstab, cryptotab, lineno );
			line = createTabLine( i->nnew );
			cur->append( line );
			}
		    else
			{
			line = (*cur)[lineno];
			list<string> ol, nl;
			makeStringList( i->nnew, nl );
			makeStringList( i->old, ol );
			list<string>::const_iterator oi = ol.begin();
			list<string>::const_iterator ni = nl.begin();
			string::size_type pos = line.find_first_not_of( " \t" );
			string::size_type posn = line.find_first_of( " \t", pos );
			posn = line.find_first_not_of( " \t", posn );
			while( ni != nl.end() )
			    {
			    y2milestone( "substr:\"%s\"", line.substr( pos, posn-pos ).c_str() );
			    if( *ni != *oi )
				{
				string nstr = *ni;
				if( posn != string::npos )
				    {
				    unsigned diff = posn-pos-1;
				    if( diff > nstr.size() )
					nstr.replace( string::npos, 0, 
						      diff-nstr.size(), ' ' );
				    line.replace( pos, posn-1, nstr );
				    if( nstr.size()>diff )
					posn += nstr.size()-diff;
				    }
				else
				    line.replace( pos, posn, nstr );
				}
			    pos = posn;
			    posn = line.find_first_of( " \t", pos );
			    posn = line.find_first_not_of( " \t", posn );
			    ++oi;
			    ++ni;
			    }
			cur[lineno] = line;
			}
		    }
		else
		    ret = FSTAB_UPDATE_ENTRY_NOT_FOUND;
		break;
	    case Entry::ADD:
		cur = findFile( i->nnew, fstab, cryptotab, lineno );
		if( lineno<0 )
		    {
		    string line = createTabLine( i->nnew );
		    cur->append( line );
		    }
		else
		    ret = FSTAB_ADD_ENTRY_FOUND;
		i->old = i->nnew;
		i->op = Entry::NONE;
		break;
	    default:
		break;
	    }
	++i;
	}
    if( fstab != NULL )
	{
	fstab->updateFile();
	delete( fstab );
	}
    if( cryptotab != NULL )
	{
	cryptotab->updateFile();
	delete( cryptotab );
	}
    y2milestone( "ret:%d", i!=co.end() );
    return( ret );
    }

string EtcFstab::addText( bool doing, bool crypto, const string& mp )
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Adding entry for mount point %1$s to %2$s"),
		       mp.c_str(), crypto?"/etc/cryptotsb":"/etc/fstab" );
	}
    else
	{
	// displayed text before action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Add entry for mount point %1$s to %2$s"),
		       mp.c_str(), crypto?"/etc/cryptotsb":"/etc/fstab" );
	}
    return( txt );
    }

string EtcFstab::updateText( bool doing, bool crypto, const string& mp )
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Updating entry for mount point %1$s in %2$s"),
		       mp.c_str(), crypto?"/etc/cryptotsb":"/etc/fstab" );
	}
    else
	{
	// displayed text before action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Update entry for mount point %1$s in %2$s"),
		       mp.c_str(), crypto?"/etc/cryptotsb":"/etc/fstab" );
	}
    return( txt );
    }

string EtcFstab::removeText( bool doing, bool crypto, const string& mp )
    {
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Removing entry for mount point %1$s from %2$s"),
		       mp.c_str(), crypto?"/etc/cryptotsb":"/etc/fstab" );
	}
    else
	{
	// displayed text before action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Remove entry for mount point %1$s from %2$s"),
		       mp.c_str(), crypto?"/etc/cryptotsb":"/etc/fstab" );
	}
    return( txt );
    }



unsigned EtcFstab::fstabFields[] = { 20, 20, 10, 21, 1, 1 };
unsigned EtcFstab::cryptotabFields[] = { 11, 15, 20, 10, 10, 1 };
string EtcFstab::blanks = "                                                                                                                        "; 

