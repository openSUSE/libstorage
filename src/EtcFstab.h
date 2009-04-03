#ifndef ETC_FSTAB_H
#define ETC_FSTAB_H

#include <string>
#include <list>
#include <map>
#include <boost/algorithm/string.hpp>

#include "y2storage/StorageInterface.h"

namespace storage
{

class AsciiFile;
struct FstabChange;

struct FstabEntry
    {
    FstabEntry() { freq=passno=0; 
                   crypto=cryptt=loop=noauto=dmcrypt=tmpcrypt=false;
                   encr=storage::ENC_NONE; mount_by=storage::MOUNTBY_DEVICE; }
    FstabEntry& operator=( const FstabChange& rhs );
    friend std::ostream& operator<< (std::ostream& s, const FstabEntry &v );

    string device;
    string dentry;
    string mount;
    string fs;
    std::list<string> opts;
    int freq;
    int passno;
    bool loop;
    bool dmcrypt;
    bool noauto;
    bool crypto;
    bool cryptt;
    bool tmpcrypt;
    string loop_dev;
    string cr_opts;
    string cr_key;
    storage::EncryptType encr;
    storage::MountByType mount_by;

    void calcDependent();
    };

inline std::ostream& operator<< (std::ostream& s, const FstabEntry &v )
    {
    s << "device:" << v.device
      << " dentry:" << v.dentry << " mount:" << v.mount
      << " fs:" << v.fs << " opts:" << boost::join( v.opts, "," )
      << " freq:" << v.freq << " passno:" << v.passno;
    if( v.noauto )
	s << " noauto";
    if( v.crypto )
	s << " crypto";
    if( v.cryptt )
	s << " cryptt";
    if( v.tmpcrypt )
	s << " tmpcrypt";
    if( v.loop )
	s << " loop";
    if( v.dmcrypt )
	s << " dmcrypt";
    if( !v.loop_dev.empty() )
	s << " loop_dev:" << v.loop_dev;
    if( !v.cr_key.empty() )
	s << " cr_key:" << v.cr_key;
    if( !v.cr_opts.empty() )
	s << " cr_opts:" << v.cr_opts;
    if( v.encr != storage::ENC_NONE )
	s << " encr:" << v.encr;
    return( s );
    }

struct FstabChange
    {
    FstabChange() { freq=passno=0; encr=storage::ENC_NONE; tmpcrypt=false; }
    FstabChange( const FstabEntry& e ) { *this = e; }
    FstabChange& operator=( const FstabEntry& rhs )
	{
	device = rhs.device;
	dentry = rhs.dentry; mount = rhs.mount; fs = rhs.fs;
	opts = rhs.opts; freq = rhs.freq; passno = rhs.passno;
	loop_dev = rhs.loop_dev; encr = rhs.encr; 
	tmpcrypt = rhs.tmpcrypt;
	return( *this );
	}
    friend std::ostream& operator<< (std::ostream& s, const FstabChange &v );
    string device;
    string dentry;
    string mount;
    string fs;
    std::list<string> opts;
    int freq;
    int passno;
    string loop_dev;
    storage::EncryptType encr;
    bool tmpcrypt;
    };

inline FstabEntry& FstabEntry::operator=( const FstabChange& rhs )
    {
    device = rhs.device;
    dentry = rhs.dentry; mount = rhs.mount; fs = rhs.fs;
    opts = rhs.opts; freq = rhs.freq; passno = rhs.passno;
    loop_dev = rhs.loop_dev; encr = rhs.encr;
    tmpcrypt = rhs.tmpcrypt;
    calcDependent();
    return( *this );
    }

inline std::ostream& operator<< (std::ostream& s, const FstabChange &v )
    {
    s << "device:" << v.device
      << " dentry:" << v.dentry << " mount:" << v.mount
      << " fs:" << v.fs << " opts:" << boost::join( v.opts, "," )
      << " freq:" << v.freq << " passno:" << v.passno;
    if( !v.loop_dev.empty() )
	s << " loop_dev:" << v.loop_dev;
    if( v.encr != storage::ENC_NONE )
	s << " encr:" << v.encr;
    if( v.tmpcrypt )
	s << " tmpcrypt";
    return( s );
    }

class EtcFstab
    {
    public:
	EtcFstab( const string& prefix = "", bool rootMounted=true );
	bool findDevice( const string& dev, FstabEntry& entry ) const;
	bool findDevice( const std::list<string>& dl, FstabEntry& entry ) const;
	bool findMount( const string& mount, FstabEntry& entry ) const;
	bool findUuidLabel( const string& uuid, const string& label,
			    FstabEntry& entry ) const;
	bool findIdPath( const std::list<string>& id, const string& path,
			 FstabEntry& entry ) const;
	void setDevice( const FstabEntry& entry, const string& device );
	int updateEntry( const string& dev, const string& mount,
	                 const string& fs, const string& opts="defaults" );
	int updateEntry( const FstabChange& entry );
	int addEntry( const FstabChange& entry );
	int removeEntry( const FstabEntry& entry );
	int changeRootPrefix( const string& prfix );
	void getFileBasedLoops( const string& prefix, std::list<FstabEntry>& l ) const;
	void getEntries( std::list<FstabEntry>& l ) const;
	string addText( bool doing, bool crypto, const string& mp ) const;
	string updateText( bool doing, bool crypto, const string& mp ) const;
	string removeText( bool doing, bool crypto, const string& mp ) const;
	int flush();
	int findPrefix( const AsciiFile& tab, const string& mount ) const;

    protected:
	struct Entry
	    {
	    enum operation { NONE, ADD, REMOVE, UPDATE };
	    Entry() { op=NONE; }
	    operation op;
	    FstabEntry nnew;
	    FstabEntry old;
	    };

	void readFiles();
	AsciiFile* findFile( const FstabEntry& e, AsciiFile*& fstab,
			     AsciiFile*& cryptotab, int& lineno ) const;
	bool findCrtab( const FstabEntry& e, const AsciiFile& crtab,
			int& lineno ) const;
	bool findCrtab( const string& device, const AsciiFile& crtab,
			int& lineno ) const;
	void makeStringList( const FstabEntry& e, std::list<string>& ls ) const;
	void makeCrtabStringList( const FstabEntry& e, std::list<string>& ls ) const;
	string updateLine( const std::list<string>& ol, 
			   const std::list<string>& nl, const string& line ) const;
	string createLine( const std::list<string>& ls, unsigned fields, 
	                   unsigned* flen ) const;
	string createTabLine( const FstabEntry& e ) const;
	void makeCrStringList( const FstabEntry& e, std::list<string>& ls ) const;
	string createCrtabLine( const FstabEntry& e ) const;

	static unsigned fstabFields[6];
	static unsigned cryptotabFields[6];
	static unsigned crypttabFields[6];

	string prefix;
	std::list<Entry> co;
    };

}

#endif
