#ifndef ETC_FSTAB_H
#define ETC_FSTAB_H

#include <string>
#include <list>
#include <map>

#include "y2storage/StorageInterface.h"

struct FstabEntry
    {
    FstabEntry() { freq=passno=0; crypto=loop=noauto=false; 
                   encr=storage::ENC_NONE; mount_by=storage::MOUNTBY_DEVICE; }
    string device;
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
    };

class EtcFstab 
    {
    public:
	EtcFstab( const string& prefix = "" );
	bool findDevice( const string& dev, FstabEntry& entry ) const;
	bool findDevice( const list<string>& dl, FstabEntry& entry ) const;
	bool findMount( const string& mount, FstabEntry& entry ) const;
	bool findUuidLabel( const string& uuid, const string& label,
			    FstabEntry& entry ) const;
	void updateEntry( const string& dev, const string& mount,
	                  const string& fs, const string& opts="defaults" );
	void updateEntry( const FstabEntry& entry );
	void keepSync( bool val=true ) { sync=val; }
	void flush();
    protected:
	struct Entry
	    {
	    enum operation { FST_NONE, FST_ADD, FST_REMOVE, FST_UPDATE };
	    Entry() { op=FST_NONE; }
	    operation op;
	    FstabEntry nnew;
	    FstabEntry old;
	    };
	bool sync;
	list<Entry> co;
    };
///////////////////////////////////////////////////////////////////

#endif
