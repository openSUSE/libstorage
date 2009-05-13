#ifndef PROC_MOUNTS_H
#define PROC_MOUNTS_H

#include <string>
#include <list>
#include <map>

#include "y2storage/EtcFstab.h"

namespace storage
{
    class Storage;
    class SystemCmd;

    class ProcMounts
    {
    public:
	ProcMounts( Storage * const s );
	string getMount( const string& Dev ) const;
	string getMount( const std::list<string>& dl ) const;
	std::map<string,string> allMounts() const;
	void getEntries( std::list<FstabEntry>& l ) const;

    protected:
	std::map<string,FstabEntry> co;

    private:

	bool isBind(SystemCmd& mt, const string& dir) const;

    };

}

#endif
