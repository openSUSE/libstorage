#ifndef BLKID_H
#define BLKID_H

#include <string>
#include <list>
#include <map>

#include "storage/Storage.h"


namespace storage
{
    using std::list;
    using std::map;


    class Blkid
    {
    public:

	Blkid();
	Blkid(const string& device);

	struct Entry
	{
	    Entry() : fstype(FSNONE), uuid(), label(), luks(false) {}

	    FsType fstype;
	    string uuid;
	    string label;
	    bool luks;
	};

	friend std::ostream& operator<<(std::ostream& s, const Entry& entry);

	bool getEntry(const string& device, Entry& entry) const;

    protected:

	void parse(const vector<string>& lines);

	typedef map<string, Entry>::const_iterator const_iterator;

	map<string, Entry> data;

    };

}


#endif
