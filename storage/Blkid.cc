
#include "storage/AppUtil.h"
#include "storage/SystemCmd.h"
#include "storage/StorageTmpl.h"
#include "storage/StorageDefines.h"
#include "storage/Blkid.h"
#include "storage/Volume.h"


namespace storage
{
    using namespace std;


    Blkid::Blkid()
    {
	SystemCmd blkid("BLKID_SKIP_CHECK_MDRAID=1 " BLKIDBIN " -c /dev/null");
	parse(blkid.stdout());
    }


    Blkid::Blkid(const string& device)
    {
	SystemCmd blkid("BLKID_SKIP_CHECK_MDRAID=1 " BLKIDBIN " -c /dev/null " + quote(device));
	parse(blkid.stdout());
    }


    void
    Blkid::parse(const vector<string>& lines)
    {
	data.clear();

	for (vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
	{
	    list<string> l = splitString(*it, " \t\n", true, true, "\"");

	    if (l.empty())
		continue;

	    string device = l.front();
	    device.erase(device.size() - 1);

	    l.pop_front();

	    Entry entry;

	    map<string, string> m = makeMap(l, "=", "\"");

	    map<string, string>::const_iterator i = m.find("TYPE");
	    if (i != m.end())
	    {
		if (i->second == "reiserfs")
		    entry.fstype = REISERFS;
		else if (i->second == "swap")
		    entry.fstype = SWAP;
		else if (i->second == "ext2")
		    entry.fstype = (m["SEC_TYPE"] == "ext3") ? EXT3 : EXT2;
		else if (i->second == "ext3")
		    entry.fstype = EXT3;
		else if (i->second == "ext4")
		    entry.fstype = EXT4;
		else if (i->second == "btrfs")
		    entry.fstype = BTRFS;
		else if (i->second == "vfat")
		    entry.fstype = VFAT;
		else if (i->second == "ntfs")
		    entry.fstype = NTFS;
		else if (i->second == "jfs")
		    entry.fstype = JFS;
		else if (i->second == "hfs")
		    entry.fstype = HFS;
		else if (i->second == "hfsplus")
		    entry.fstype = HFSPLUS;
		else if (i->second == "xfs")
		    entry.fstype = XFS;
		else if (i->second == "(null)")
		    entry.fstype = FSNONE;
		else if (i->second == "crypto_LUKS")
		    entry.luks = true;
	    }

	    i = m.find("UUID");
	    if (i != m.end())
		entry.uuid = i->second;

	    i = m.find("LABEL");
	    if (i != m.end())
		entry.label = i->second;

	    data[device] = entry;
	}

	for (const_iterator it = data.begin(); it != data.end(); ++it)
	    y2mil("data[" << it->first << "] -> " << it->second);
    }


    bool
    Blkid::getEntry(const string& device, Entry& entry) const
    {
	const_iterator i = data.find(device);
	if (i == data.end())
	    return false;

	entry = i->second;
	return true;
    }


    std::ostream& operator<<(std::ostream& s, const Blkid::Entry& entry)
    {
	s << "fstype:" << Volume::fsTypeString(entry.fstype);
	if (!entry.uuid.empty())
	    s << " uuid:" << entry.uuid;
	if (!entry.label.empty())
	    s << " label:" << entry.label;
	if (entry.luks)
	    s << " luks:" << entry.luks;
	return s;
    }

}
