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
    bool operator==( const FstabEntry& rhs ) const
	{ return( dentry == rhs.dentry && mount == rhs.mount ); }
    bool operator==( const FstabChange& rhs ) const;
    bool operator!=( const FstabEntry& rhs ) const
	{ return( ! (*this == rhs) ); }
    bool operator!=( const FstabChange& rhs ) const;
    FstabEntry& operator=( const FstabChange& rhs );

    string device;
    string dentry;
    string mount;
    string fs;
    list<string> opts;
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

struct FstabChange
    {
    FstabChange() { freq=passno=0; encr=storage::ENC_NONE; }
    FstabChange( const FstabEntry& e ) { *this = e; }
    bool operator==( const FstabChange& rhs ) const
	{ return( dentry == rhs.dentry && mount == rhs.mount ); }
    bool operator==( const FstabEntry& rhs ) const
	{ return( dentry == rhs.dentry && mount == rhs.mount ); }
    bool operator!=( const FstabEntry& rhs ) const
	{ return( ! (*this == rhs) ); }
    bool operator!=( const FstabChange& rhs ) const
	{ return( ! (*this == rhs) ); }
    FstabChange& operator=( const FstabEntry& rhs )
	{
	dentry = rhs.dentry; mount = rhs.mount; fs = rhs.fs;
	opts = rhs.opts; freq = rhs.freq; passno = rhs.passno;
	loop_dev = rhs.loop_dev; encr = rhs.encr;
	return( *this );
	}
    string dentry;
    string mount;
    string fs;
    list<string> opts;
    int freq;
    int passno;
    string loop_dev;
    storage::EncryptType encr;
    };

inline bool FstabEntry::operator==( const FstabChange& rhs ) const
    { return( dentry == rhs.dentry && mount == rhs.mount ); }

inline bool FstabEntry::operator!=( const FstabChange& rhs ) const
    { return( ! (*this == rhs) ); }

inline FstabEntry& FstabEntry::operator=( const FstabChange& rhs )
    {
    dentry = rhs.dentry; mount = rhs.mount; fs = rhs.fs;
    opts = rhs.opts; freq = rhs.freq; passno = rhs.passno;
    loop_dev = rhs.loop_dev; encr = rhs.encr;
    calcDependent();
    return( *this );
    }


class EtcFstab 
    {
    public:
	EtcFstab( const string& prefix = "" );
	bool findDevice( const string& dev, FstabEntry& entry ) const;
	bool findDevice( const list<string>& dl, FstabEntry& entry ) const;
	bool findMount( const string& mount, FstabEntry& entry ) const;
	bool findUuidLabel( const string& uuid, const string& label,
			    FstabEntry& entry ) const;
	void setDevice( const FstabEntry& entry, const string& device );
	int updateEntry( const string& dev, const string& mount,
	                 const string& fs, const string& opts="defaults" );
	int updateEntry( const FstabChange& entry );
	int addEntry( const FstabChange& entry );
	int removeEntry( const FstabEntry& entry );
	void keepSync( bool val=true ) { sync=val; }
	int changeRootPrefix( const string& prfix );
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

	AsciiFile* findFile( const FstabEntry& e, AsciiFile*& fstab,
			     AsciiFile*& cryptotab, int& lineno );
	void makeStringList( const FstabEntry& e, list<string>& ls );
	string createTabLine( const FstabEntry& e );

	static unsigned fstabFields[6];
	static unsigned cryptotabFields[6];
	static string blanks;

	string prefix;
	bool sync;
	list<Entry> co;
    };
///////////////////////////////////////////////////////////////////

#endif
