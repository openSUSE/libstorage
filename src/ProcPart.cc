// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/


#include "storage/AppUtil.h"
#include "storage/StorageTmpl.h"
#include "storage/AsciiFile.h"
#include "storage/ProcPart.h"


namespace storage
{
    using namespace std;


    ProcPart::ProcPart()
    {
	reload();
    }


    void
    ProcPart::reload()
    {
	data.clear();

	AsciiFile parts("/proc/partitions");
	parts.remove(0, 2);

	for (vector<string>::const_iterator it = parts.lines().begin(); it != parts.lines().end(); ++it)
	{
	    string device = extractNthWord(3, *it);
	    unsigned long long sizeK;
	    extractNthWord(2, *it) >> sizeK;
	    data[device] = sizeK;
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> " << it->second);
    }


    bool
    ProcPart::getSize(const string& device, unsigned long long& sizeK) const
    {
	bool ret = false;
	const_iterator i = data.find(undevDevice(device));
	if (i != data.end())
	{
	    sizeK = i->second;
	    ret = true;
	}
	y2mil("dev:" << device << " ret:" << ret << " sizeK:" << (ret ? sizeK : 0));
	return ret;
    }


    list<string>
    ProcPart::getEntries() const
    {
	list<string> ret;
	for (const_iterator i = data.begin(); i != data.end(); ++i)
	    ret.push_back(i->first);
	return ret;
    }

}
