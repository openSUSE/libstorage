#include <iostream> 
#include <sstream> 

#include <ycp/y2log.h>

#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AppUtil.h"

LvmVg::LvmVg( Storage * const s, const string& Name ) :
    Container(s,Name,staticType())
    {
    y2milestone( "constructed lvm vg %s", dev.c_str() );
    }

LvmVg::LvmVg( Storage * const s, const string& file, bool ) :
    Container(s,"",staticType())
    {
    y2milestone( "constructed lvm vg %s from file %s", dev.c_str(), 
                 file.c_str() );
    }

LvmVg::~LvmVg()
    {
    y2milestone( "destructed lvm vg %s", dev.c_str() );
    }

void LvmVg::activate( bool val )
    {
    if( active!=val )
	{
	SystemCmd c;
	if( active )
	    {
	    c.execute( "vgscan --mknodes" );
	    c.execute( "vgchange -a y" );
	    }
	else
	    {
	    c.execute( "vgchange -a n" );
	    }
	active = val;
	}
    }

void LvmVg::getVgs( list<string>& l )
    {
    l.clear();
    string vgname;
    string::size_type pos;
    SystemCmd c( "vgdisplay -s" );
    if( !active && c.numLines()>0 )
	active = true;
    for( int i=0; i<c.numLines(); ++i )
	{
	vgname = *c.getLine(i);
	pos=vgname.find_first_not_of( " \t\n\"" );
	if( pos>0 )
	    vgname.erase( 0, pos );
	pos=vgname.find_first_of( " \t\n\"" );
	if( pos>0 )
	    vgname.erase( pos );
	l.push_back(vgname);
	}
    std::ostringstream buf;
    buf << l;
    y2milestone( "detecte Vgs %s", buf.str().c_str() );
    }

int LvmVg::doCreate( Volume* v ) { return( 0 ); }
int LvmVg::doRemove( Volume* v ) { return( 0 ); }
int LvmVg::doResize( Volume* v ) { return( 0 ); }

int
LvmVg::commitChanges( CommitStage stage )
    {
    return( 0 );
    }

void LvmVg::getCommitActions( list<commitAction*>& l ) const
    {
    }

int
LvmVg::checkResize( Volume* v, unsigned long long newSizeK ) const
    {
    return( false );
    }

void LvmVg::logData( const string& Dir ) {;}

bool LvmVg::active = false;

