#ifndef PROC_MOUNTS_H
#define PROC_MOUNTS_H

#include <string>
#include <list>
#include <map>

#include "y2storage/EtcFstab.h"

namespace storage
{
    using std::list;
    using std::map;
    using std::multimap;

    class Storage;
    class SystemCmd;

    class ProcMounts
    {
    public:

	ProcMounts( Storage * const s );

	string getMount(const string& device) const;
	string getMount(const list<string>& devices) const;

	list<string> getAllMounts(const string& device) const;
	list<string> getAllMounts(const list<string>& devices) const;

	map<string, string> allMounts() const;

	list<FstabEntry> getEntries() const;

    protected:

	bool isBind(SystemCmd& mt, const string& dir) const;

	typedef multimap<string, FstabEntry>::const_iterator const_iterator;

	multimap<string, FstabEntry> data;

    };

}

#endif
