#ifndef ETC_FSTAB_H
#define ETC_FSTAB_H

#include <string>
#include <list>
#include <map>

#include "y2storage/StorageInterface.h"

struct FstabEntry
    {
    FstabEntry() { freq=passno=0; crypt=noauto=false; encr=storage::ENC_NONE; }
    string device;
    string mount;
    string fs;
    list<string> opts;
    int freq;
    int passno;
    bool crypt;
    bool noauto;
    string loop;
    storage::EncryptType encr;
    };

class EtcFstab 
    {
    public:
	EtcFstab();
	bool findDevice( const string& dev, FstabEntry& entry );
	bool findMount( const string& mount, FstabEntry& entry );
	void updateEntry( const string& dev, const string& mount,
	                  const string& fs, const string& opts="defaults" );
	void updateEntry( const FstabEntry& entry );
	void keepSync( bool val=true ) { sync=val; }
	void flush();
    protected:
	struct entry
	    {
	    bool added;
	    FstabEntry nnew;
	    FstabEntry old;
	    };
	bool sync;
	list<entry> co;
    };
///////////////////////////////////////////////////////////////////

#endif
