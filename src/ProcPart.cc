// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/


#include "y2storage/AppUtil.h"
#include "y2storage/Regex.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/AsciiFile.h"
#include "y2storage/ProcPart.h"


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

	AsciiFile file("/proc/partitions");
	const vector<string>& lines = file.lines();

	for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	{
	    string device = extractNthWord(3, *it);
	    if (!device.empty() && device != "name")
	    {
		unsigned long long sizeK;
		extractNthWord(2, *it) >> sizeK;
		data[device] = sizeK;
	    }
	}
    }


    bool
    ProcPart::getSize(const string& device, unsigned long long& sizeK) const
    {
	bool ret = false;
	map<string, unsigned long long>::const_iterator i = data.find(undevDevice(device));
	if (i != data.end())
	{
	    sizeK = i->second;
	    ret = true;
	}
	y2mil("dev:" << device << " ret:" << ret << " sizeK:" << (ret ? sizeK : 0));
	return ret;
    }


    list<string>
    ProcPart::getMatchingEntries(const string& regexp) const
    {
	Regex reg("^" + regexp + "$");
	list<string> ret;
	for (map<string, unsigned long long>::const_iterator i = data.begin(); i != data.end(); ++i)
	{
	    if (reg.match(i->first))
		ret.push_back(i->first);
	}
	return ret;
    }

}
