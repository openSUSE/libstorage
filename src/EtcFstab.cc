// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/


#include <sstream>
#include <algorithm>

#include "y2storage/AppUtil.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/Regex.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/Volume.h"
#include "y2storage/EtcFstab.h"

using namespace storage;
using namespace std;

EtcFstab::EtcFstab( const string& pfx, bool rootMounted ) : prefix(pfx)
    {
    y2milestone( "prefix:%s rootMounted:%d", pfx.c_str(), rootMounted );
    if( rootMounted )
	readFiles();
    }

void
EtcFstab::readFiles()
    {
    string file = prefix+"/fstab";
    ifstream mounts( file.c_str() );
    string line;
    unsigned lineno = 0;
    getline( mounts, line );
    list<Entry>::iterator i = co.begin();
    while( i!=co.end() )
	{
	if( i->op!=Entry::ADD )
	    i=co.erase(i);
	else
	    ++i;
	}
    y2milestone( "entries:%zd", co.size() );
    while( mounts.good() )
	{
	y2mil( "line:\"" << line << "\"" );
	lineno++;
	list<string> l = splitString( line );
	list<string>::const_iterator i = l.begin();
	if( l.begin()!=l.end() && i->find( '#' )!=0 )
	    {
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
	    p->old.calcDependent();
	    p->nnew = p->old;
	    co.push_back( *p );
	    delete p;
	    }
	getline( mounts, line );
	}
    mounts.close();
    y2milestone( "file:%s lines:%u", file.c_str(), lineno );
    lineno=0;
    file = prefix+"/cryptotab";
    mounts.clear();
    mounts.open( file.c_str() );
    getline( mounts, line );
    while( mounts.good() )
	{
	y2mil( "line:\"" << line << "\"" );
	lineno++;
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
    y2milestone( "file:%s lines:%u", file.c_str(), lineno );
    y2milestone( "entries:%zd", co.size() );
    }

void
FstabEntry::calcDependent()
    {
    list<string>::const_iterator beg = opts.begin();
    list<string>::const_iterator end = opts.end();

    noauto = find( beg, end, "noauto" ) != end;

    list<string>::const_iterator i = find_if( beg, end, find_begin("loop") );
    if( i!=end )
	{
	loop = true;
	string::size_type pos = i->find("=");
	if( pos!=string::npos )
	    {
	    loop_dev = i->substr( pos+1 );
	    }
	}
    i = find_if( beg, end, find_begin("encryption=") );
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
    else if( device.substr(0, 16) == "/dev/disk/by-id/" )
        {
	mount_by = MOUNTBY_ID;
	device.erase();
	}
    else if( device.substr(0, 18) == "/dev/disk/by-path/" )
        {
	mount_by = MOUNTBY_PATH;
	device.erase();
	}

    crypto = !noauto && encr!=ENC_NONE;
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
    y2milestone( "new prefix:%s", prfix.c_str() );
    int ret = 0;
    if( prfix != prefix )
	{
	list<Entry>::const_iterator i = co.begin();
	while( i!=co.end() && (i->op==Entry::ADD || i->op==Entry::NONE) )
	    ++i;
	if( i==co.end() )
	    {
	    prefix = prfix;
	    readFiles();
	    }
	else
	    ret = FSTAB_CHANGE_PREFIX_IMPOSSIBLE;
	}
    y2milestone( "ret:%d", ret );
    return( ret );
    }

void EtcFstab::setDevice( const FstabEntry& entry, const string& device )
    {
    list<Entry>::iterator i = co.begin();
    while( i!=co.end() && i->old.dentry != entry.dentry )
	++i;
    if( i!=co.end() )
	i->nnew.device = i->old.device = device;
    }

bool
EtcFstab::findUuidLabel( const string& uuid, const string& label,
                         FstabEntry& entry ) const
    {
    y2milestone( "uuid:%s label:%s", uuid.c_str(), label.c_str() );
    list<Entry>::const_iterator i = co.end();
    if( !uuid.empty() )
	{
	string dev = "UUID=" + uuid;
	i = co.begin();
	while( i!=co.end() && i->nnew.dentry != dev )
	    ++i;
	}
    if( i==co.end() && !label.empty() )
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

bool
EtcFstab::findIdPath( const std::list<string>& id, const string& path,
		      FstabEntry& entry ) const
    {
    y2mil( "id:" << id << " path:" << path );
    list<Entry>::const_iterator i = co.begin();
    if( !id.empty() )
	{
	while( i!=co.end() && 
	       find( id.begin(), id.end(), i->nnew.dentry )==id.end() )
	    ++i;
	}
    else 
	i = co.end();
    if( i==co.end() && !path.empty() )
	{
	i = co.begin();
	while( i!=co.end() && i->nnew.dentry != path )
	    ++i;
	}
    if( i!=co.end())
	entry = i->nnew;
    y2milestone( "ret:%d", i!=co.end() );
    return( i!=co.end() );
    }

int EtcFstab::removeEntry( const FstabEntry& entry )
    {
    y2milestone( "dev:%s mp:%s", entry.dentry.c_str(), entry.mount.c_str() );
    list<Entry>::iterator i = co.begin();
    while( i != co.end() &&
           (i->op==Entry::REMOVE || i->nnew.device != entry.device) )
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
    y2milestone( "dev:%s mp:%s", entry.dentry.c_str(), entry.mount.c_str() );
    list<Entry>::iterator i = co.begin();
    bool found = false;
    while( i != co.end() && !found )
	{
	if( i->op==Entry::REMOVE ||
	    (i->op!=Entry::ADD && entry.device!=i->old.device) ||
	    (i->op==Entry::ADD && entry.device!=i->nnew.device) )
	    ++i;
	else
	    found = true;
	}
    if( i != co.end() )
	{
	if( i->op==Entry::NONE )
	    i->op = Entry::UPDATE;
	i->nnew = entry;
	}
    int ret = (i != co.end())?0:FSTAB_ENTRY_NOT_FOUND;
    y2milestone( "ret:%d", ret );
    return( ret );
    }

int EtcFstab::addEntry( const FstabChange& entry )
    {
    y2milestone( "dev:%s mp:%s", entry.dentry.c_str(), entry.mount.c_str() );
    Entry e;
    e.op = Entry::ADD;
    e.nnew = entry;
    co.push_back( e );
    return( 0 );
    }

AsciiFile* EtcFstab::findFile( const FstabEntry& e, AsciiFile*& fstab,
                               AsciiFile*& cryptotab, int& lineno )
    {
    y2milestone( "device:%s mp:%s fstab:%p cryptotab:%p", e.dentry.c_str(),
                 e.mount.c_str(), fstab, cryptotab );
    AsciiFile* ret=NULL;
    Regex *fi = NULL;
    if( e.crypto )
	{
	if( cryptotab==NULL )
	    cryptotab = new AsciiFile( prefix + "/cryptotab" );
	ret = cryptotab;
	fi = new Regex( "[ \t]" + e.dentry + "[ \t]" );
	}
    else
	{
	if( fstab==NULL )
	    fstab = new AsciiFile( prefix + "/fstab" );
	ret = fstab;
	fi = new Regex( "^[ \t]*" + e.dentry + "[ \t]" );
	}
    lineno = ret->find( 0, *fi );
    delete fi;
    y2milestone( "fstab:%p cryptotab:%p lineno:%d", fstab, cryptotab, lineno );
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
    y2milestone( "device:%s mp:%s", e.dentry.c_str(), e.mount.c_str() );
    string ret;
    list<string> ls;
    makeStringList( e, ls );
    int count=0;
    int max_fields = e.crypto ? lengthof(cryptotabFields)
			      : lengthof(fstabFields);
    unsigned * fields = e.crypto ? cryptotabFields : fstabFields;
    for( list<string>::const_iterator i=ls.begin(); i!=ls.end(); ++i )
	{
	if( i != ls.begin() )
	    ret += " ";
	ret += *i;
	if( count<max_fields && i->size()<fields[count] )
	    {
	    ret.replace( ret.size(), 0, fields[count]-i->size(), ' ' );
	    }
	count++;
	}
    y2milestone( "ret:%s", ret.c_str() );
    return( ret );
    }

void EtcFstab::getFileBasedLoops( const string& prefix, list<FstabEntry>& l )
    {
    l.clear();
    list<Entry>::iterator i = co.begin();
    while( i!=co.end() )
	{
	if( i->op==Entry::NONE )
	    {
	    string lfile = prefix + i->old.dentry;
	    if( checkNormalFile( lfile ))
		l.push_back( i->old );
	    }
	++i;
	}
    }

void EtcFstab::getEntries( list<FstabEntry>& l )
    {
    l.clear();
    list<Entry>::iterator i = co.begin();
    while( i!=co.end() )
	{
	if( i->op==Entry::NONE )
	    {
	    l.push_back( i->old );
	    }
	++i;
	}
    }

int EtcFstab::flush()
    {
    int ret = 0;
    list<Entry>::iterator i = co.begin();
    AsciiFile *fstab = NULL;
    AsciiFile *cryptotab = NULL;
    AsciiFile *cur = NULL;
    int lineno;
    if( i!=co.end() && !checkDir( prefix ) )
	createPath( prefix );
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
		if( lineno<0 )
		    cur = findFile( i->nnew, fstab, cryptotab, lineno );
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
			string::size_type pos =
			    line.find_first_not_of( app_ws );
			string::size_type posn =
			    line.find_first_of( app_ws, pos );
			posn = line.find_first_not_of( app_ws, posn );
			while( ni != nl.end() )
			    {
			    if( *ni != *oi )
				{
				string nstr = *ni;
				if( posn != string::npos )
				    {
				    unsigned diff = posn-pos-1;
				    if( diff > nstr.size() )
					{
					nstr.replace( nstr.size(), 0,
						      diff-nstr.size(), ' ' );
					}
				    line.replace( pos, posn-1-pos, nstr );
				    if( nstr.size()>diff )
					posn += nstr.size()-diff;
				    }
				else
				    {
				    line.replace( pos, posn-pos, nstr );
				    }
				}
			    pos = posn;
			    posn = line.find_first_of( app_ws, pos );
			    posn = line.find_first_not_of( app_ws, posn );
			    ++oi;
			    ++ni;
			    }
			(*cur)[lineno] = line;
			}
		    i->old = i->nnew;
		    i->op = Entry::NONE;
		    }
		else
		    ret = FSTAB_UPDATE_ENTRY_NOT_FOUND;
		break;
	    case Entry::ADD:
		{
		cur = findFile( i->nnew, fstab, cryptotab, lineno );
		string line = createTabLine( i->nnew );
		if( lineno<0 )
		    {
		    cur->append( line );
		    }
		else
		    {
		    y2war( "replacing line:" << (*cur)[lineno] );
		    (*cur)[lineno] = line;
		    }
		i->old = i->nnew;
		i->op = Entry::NONE;
		}
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
    y2milestone( "ret:%d", ret );
    return( ret );
    }

string EtcFstab::addText( bool doing, bool crypto, const string& mp )
    {
    const char* file = crypto?"/etc/cryptotab":"/etc/fstab";
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Adding entry for mount point %1$s to %2$s"),
		       mp.c_str(), file );
	}
    else
	{
	// displayed text before action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Add entry for mount point %1$s to %2$s"),
		       mp.c_str(), file );
	}
    return( txt );
    }

string EtcFstab::updateText( bool doing, bool crypto, const string& mp )
    {
    const char* file = crypto?"/etc/cryptotab":"/etc/fstab";
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Updating entry for mount point %1$s in %2$s"),
		       mp.c_str(), file );
	}
    else
	{
	// displayed text before action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Update entry for mount point %1$s in %2$s"),
		       mp.c_str(), file );
	}
    return( txt );
    }

string EtcFstab::removeText( bool doing, bool crypto, const string& mp )
    {
    const char* file = crypto?"/etc/cryptotab":"/etc/fstab";
    string txt;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Removing entry for mount point %1$s from %2$s"),
		       mp.c_str(), file );
	}
    else
	{
	// displayed text before action, %1$s is replaced by mount point e.g. /home
	// %2$s is replaced by a pathname e.g. /etc/fstab
	txt = sformat( _("Remove entry for mount point %1$s from %2$s"),
		       mp.c_str(), file );
	}
    return( txt );
    }



unsigned EtcFstab::fstabFields[] = { 20, 20, 10, 21, 1, 1 };
unsigned EtcFstab::cryptotabFields[] = { 11, 15, 20, 10, 10, 1 };

