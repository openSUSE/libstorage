#ifndef ETC_FSTAB_H
#define ETC_FSTAB_H

#include <string>
#include <list>
#include <map>

#include "y2storage/StorageInterface.h"

class AsciiFile;
struct FstabChange;

struct FstabEntry
    {
    FstabEntry() { freq=passno=0; crypto=loop=noauto=false; 
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
    bool noauto;
    bool crypto;
    string loop_dev;
    storage::EncryptType encr;
    storage::MountByType mount_by;

    void calcDependent();
    };

inline std::ostream& operator<< (std::ostream& s, const FstabEntry &v )
    {
    s << "device:" << v.device 
      << " dentry:" << v.dentry << " mount:" << v.mount 
      << " fs:" << v.fs << " opts:" << mergeString( v.opts, "," )
      << " freq:" << v.freq << " passno:" << v.passno;
    if( v.noauto )
	s << " noauto";
    if( v.crypto )
	s << " crypto";
    if( v.loop )
	s << " loop";
    if( !v.loop_dev.empty() )
	s << " loop_dev:" << v.loop_dev;
    if( v.encr != storage::ENC_NONE )
	s << " encr:" << v.encr;
    return( s );
    }

struct FstabChange
    {
    FstabChange() { freq=passno=0; encr=storage::ENC_NONE; }
    FstabChange( const FstabEntry& e ) { *this = e; }
    FstabChange& operator=( const FstabEntry& rhs )
	{
	device = rhs.device;
	dentry = rhs.dentry; mount = rhs.mount; fs = rhs.fs;
	opts = rhs.opts; freq = rhs.freq; passno = rhs.passno;
	loop_dev = rhs.loop_dev; encr = rhs.encr;
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
    };

inline FstabEntry& FstabEntry::operator=( const FstabChange& rhs )
    {
    device = rhs.device;
    dentry = rhs.dentry; mount = rhs.mount; fs = rhs.fs;
    opts = rhs.opts; freq = rhs.freq; passno = rhs.passno;
    loop_dev = rhs.loop_dev; encr = rhs.encr;
    calcDependent();
    return( *this );
    }

inline std::ostream& operator<< (std::ostream& s, const FstabChange &v )
    {
    s << "device:" << v.device 
      << " dentry:" << v.dentry << " mount:" << v.mount 
      << " fs:" << v.fs << " opts:" << mergeString( v.opts, "," )
      << " freq:" << v.freq << " passno:" << v.passno;
    if( !v.loop_dev.empty() )
	s << " loop_dev:" << v.loop_dev;
    if( v.encr != storage::ENC_NONE )
	s << " encr:" << v.encr;
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
	void setDevice( const FstabEntry& entry, const string& device );
	int updateEntry( const string& dev, const string& mount,
	                 const string& fs, const string& opts="defaults" );
	int updateEntry( const FstabChange& entry );
	int addEntry( const FstabChange& entry );
	int removeEntry( const FstabEntry& entry );
	int changeRootPrefix( const string& prfix );
	void getFileBasedLoops( const string& prefix, std::list<FstabEntry>& l );
	string addText( bool doing, bool crypto, const string&  mp );
	string updateText( bool doing, bool crypto, const string&  mp );
	string removeText( bool doing, bool crypto, const string&  mp );
	int flush();

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
			     AsciiFile*& cryptotab, int& lineno );
	void makeStringList( const FstabEntry& e, std::list<string>& ls );
	string createTabLine( const FstabEntry& e );

	static unsigned fstabFields[6];
	static unsigned cryptotabFields[6];

	string prefix;
	std::list<Entry> co;
    };
///////////////////////////////////////////////////////////////////

#endif
