/*
 * Copyright (c) [2004-2009] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include <sys/stat.h>
#include <sstream>

#include "storage/Loop.h"
#include "storage/StorageTypes.h"
#include "storage/Container.h"
#include "storage/AppUtil.h"
#include "storage/SystemInfo.h"
#include "storage/Storage.h"
#include "storage/SystemCmd.h"
#include "storage/StorageDefines.h"


namespace storage
{
    using namespace std;


Loop::Loop(const LoopCo& d, const string& LoopDev, const string& LoopFile,
	       bool dmcrypt, const string& dm_dev, SystemInfo& systeminfo,
	   SystemCmd& losetup)
    : Volume(d, 0, 0), lfile(LoopFile), reuseFile(true), delFile(false)
{
    y2mil("constructed loop dev:" << LoopDev << " file:" << LoopFile <<
	  " dmcrypt:" << dmcrypt << " dmdev:" << dm_dev);
    if( d.type() != LOOP )
	y2err("constructed loop with wrong container");
    loop_dev = fstab_loop_dev = LoopDev;
    if( loop_dev.empty() )
	getLoopData( losetup );
    if( loop_dev.empty() )
	getFreeLoop(losetup, d.usedNumbers());
    string proc_dev;
    if( !dmcrypt )
	{
	dev = loop_dev;
	if( loopStringNum( loop_dev, num ))
	    {
	    setNameDev();
	    getMajorMinor();
	    }
	proc_dev = loop_dev;
	}
    else
	{
	numeric = false;
	initEncryption( ENC_LUKS );
	if( !dm_dev.empty() )
	    {
	    setDmcryptDev( dm_dev );
	    proc_dev = alt_names.front();
	    }
	}
    is_loop = true;
    unsigned long long s = 0;
    if( !proc_dev.empty() )
	    systeminfo.getProcParts().getSize(proc_dev, s);
    if( s>0 )
	{
	setSize( s );
	}
    else
	{
	struct stat st;
	if( stat( lfile.c_str(), &st )>=0 )
	    setSize( st.st_size/1024 );
	}
}


Loop::Loop(const LoopCo& d, const string& file, bool reuseExisting,
	   unsigned long long sizeK, bool dmcr)
    : Volume(d, 0, 0), lfile(file), reuseFile(reuseExisting), delFile(false)
{
    y2mil("constructed loop file:" << file << " reuseExisting:" << reuseExisting <<
	  " sizek:" << sizeK << " dmcrypt:" << dmcr);
    if( d.type() != LOOP )
	y2err("constructed loop with wrong container");
    getFreeLoop();
    is_loop = true;
    if( !dmcr )
	{
	dev = loop_dev;
	if( loopStringNum( dev, num ))
	    {
	    setNameDev();
	    getMajorMinor();
	    }
	}
    else
	{
	numeric = false;
	initEncryption( ENC_LUKS );
	if( dmcrypt_dev.empty() )
	    dmcrypt_dev = getDmcryptName();
	setDmcryptDev( dmcrypt_dev, false );
	}
    checkReuse();
    if( !reuseFile )
	setSize( sizeK );
}


    Loop::Loop(const LoopCo& c, const Loop& v)
	: Volume(c, v), lfile(v.lfile), reuseFile(v.reuseFile),
	  delFile(v.delFile)
    {
	y2deb("copy-constructed Loop from " << v.dev);
    }


    Loop::~Loop()
    {
	y2deb("destructed Loop " << dev);
    }


void
Loop::setDmcryptDev( const string& dm_dev, bool active )
    {
    dev = dm_dev;
    y2mil( "dm_dev:" << dm_dev << " active:" << active );
    nm = dm_dev.substr( dm_dev.find_last_of( '/' )+1);
    if( active )
	{
	getMajorMinor();
	replaceAltName( "/dev/dm-", "/dev/dm-"+decString(mnr) );
	}
    Volume::setDmcryptDev( dm_dev, active );
    }

void Loop::setLoopFile( const string& file )
    {
    lfile = file;
    checkReuse();
    }

void Loop::setReuse( bool reuseExisting )
    {
    reuseFile = reuseExisting;
    checkReuse();
    }

void Loop::checkReuse()
    {
    if( reuseFile )
	{
	struct stat st;
	if( stat( lfileRealPath().c_str(), &st )>=0 )
	    setSize( st.st_size/1024 );
	else
	    reuseFile = false;
	y2mil( "reuseFile:" << reuseFile << " size:" << sizeK() );
	}
    }

bool
Loop::removeFile()
    {
    bool ret = true;
    if( delFile )
	{
	ret = unlink( lfileRealPath().c_str() )==0;
	}
    return( ret );
    }

bool
Loop::createFile()
    {
    bool ret = true;
    if( !reuseFile )
	{
	string pa = lfileRealPath();
	string::size_type pos;
	if( (pos=pa.rfind( '/' ))!=string::npos )
	    {
	    pa.erase( pos );
	    y2mil( "pa:" << pa );
	    createPath( pa );
	    }
	string cmd = DDBIN " if=/dev/zero of=" + quote(lfileRealPath());
	cmd += " bs=1k count=" + decString( sizeK() );
	SystemCmd c( cmd );
	ret = c.retcode()==0;
	}
    return( ret );
    }

string
Loop::lfileRealPath() const
    {
    return getStorage()->root() + lfile;
    }


unsigned Loop::loopMajor()
    {
    if( loop_major==0 )
    {
	loop_major = getMajorDevices("loop");
	y2mil("loop_major:" << loop_major);
    }
    return( loop_major );
    }


string Loop::loopDeviceName( unsigned num )
    {
    return( "/dev/loop" + decString(num));
    }

int Loop::setEncryption( bool val, storage::EncryptType typ )
    {
    y2mil("val:" << val << " enc_type:" << toString(typ));
    int ret = Volume::setEncryption( val, typ );
    if( ret==0 && encryption!=orig_encryption )
	{
	numeric = !dmcrypt();
	if( dmcrypt() )
	    {
	    if( dmcrypt_dev.empty() )
		dmcrypt_dev = getDmcryptName();
	    setDmcryptDev( dmcrypt_dev, false );
	    }
	else
	    {
	    dev = loop_dev;
	    }
	}
    y2mil( "ret:" << ret );
    return(ret);
    }

Text Loop::removeText( bool doing ) const
    {
    Text txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/loop0
	txt = sformat( _("Deleting file-based device %1$s"), d.c_str() );
	}
    else
	{
	// displayed text before action, %1$s is replaced by device name
	// %2$s is replaced by size (e.g. 623.5 MB)
	txt = sformat( _("Delete file-based device %1$s (%2$s)"), d.c_str(),
		       sizeString().c_str() );
	}
    return( txt );
    }

Text Loop::createText( bool doing ) const
    {
    Text txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/loop0
	// %2$s is replaced by filename (e.g. /var/adm/secure)
	txt = sformat( _("Creating file-based device %1$s of file %2$s"), d.c_str(), lfile.c_str() );
	}
    else
	{
	d.erase( 0, d.find_last_of('/')+1 );
	if( d.find( "loop" )==0 )
	    d.erase( 0, 4 );
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create file-based device %1$s of file %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Create encrypted file-based device %1$s of file %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name
	    // %2$s is replaced by filename (e.g. /var/adm/secure)
	    // %3$s is replaced by size (e.g. 623.5 MB)
	    txt = sformat( _("Create file-based device %1$s of file %2$s (%3$s)"),
			   dev.c_str(), lfile.c_str(), sizeString().c_str() );
	    }
	}
    return( txt );
    }


Text Loop::formatText( bool doing ) const
    {
    Text txt;
    string d = dev;
    if( doing )
	{
	// displayed text during action, %1$s is replaced by device name e.g. /dev/system/var
	// %2$s is replaced by filename (e.g. /var/adm/secure)
	// %3$s is replaced by size (e.g. 623.5 MB)
	// %4$s is replaced by file system type (e.g. reiserfs)
	txt = sformat( _("Formatting file-based device %1$s of %2$s (%3$s) with %4$s "),
		       d.c_str(), lfile.c_str(), sizeString().c_str(),
		       fsTypeString().c_str() );
	}
    else
	{
	d.erase( 0, d.find_last_of('/')+1 );
	if( d.find( "loop" )==0 )
	    d.erase( 0, 4 );
	if( !mp.empty() )
	    {
	    if( encryption==ENC_NONE )
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format file-based device %1$s of %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    else
		{
		// displayed text before action, %1$s is replaced by device name
		// %2$s is replaced by filename (e.g. /var/adm/secure)
		// %3$s is replaced by size (e.g. 623.5 MB)
		// %4$s is replaced by file system type (e.g. reiserfs)
		// %5$s is replaced by mount point (e.g. /usr)
		txt = sformat( _("Format encrypted file-based device %1$s of %2$s (%3$s) as %5$s with %4$s"),
			       d.c_str(), lfile.c_str(), sizeString().c_str(),
			       fsTypeString().c_str(), mp.c_str() );
		}
	    }
	else
	    {
	    // displayed text before action, %1$s is replaced by device name
	    // %2$s is replaced by filename (e.g. /var/adm/secure)
	    // %3$s is replaced by size (e.g. 623.5 MB)
	    // %4$s is replaced by file system type (e.g. reiserfs)
	    txt = sformat( _("Format file-based device %1$s of %2$s (%3$s) with %4$s"),
			   d.c_str(), lfile.c_str(), sizeString().c_str(),
			   fsTypeString().c_str() );
	    }
	}
    return( txt );
    }

void Loop::getInfo( LoopInfo& tinfo ) const
    {
    Volume::getInfo(info.v);
    info.nr = num;
    info.file = lfile;
    info.reuseFile = reuseFile;
    tinfo = info;
    }


std::ostream& operator<< (std::ostream& s, const Loop& l )
    {
    s << "Loop " << dynamic_cast<const Volume&>(l)
      << " LoopFile:" << l.lfile;
    if( l.reuseFile )
      s << " reuse";
    if( l.delFile )
      s << " delFile";
    return( s );
    }


bool Loop::equalContent( const Loop& rhs ) const
    {
    return( Volume::equalContent(rhs) &&
	    lfile==rhs.lfile && reuseFile==rhs.reuseFile &&
	    delFile==rhs.delFile );
    }


    void
    Loop::logDifference(std::ostream& log, const Loop& rhs) const
    {
	Volume::logDifference(log, rhs);

	logDiff(log, "loopfile", lfile, rhs.lfile);
	logDiff(log, "reusefile", reuseFile, rhs.reuseFile);
	logDiff(log, "delfile", delFile, rhs.delFile);
    }


unsigned Loop::loop_major = 0;

}
