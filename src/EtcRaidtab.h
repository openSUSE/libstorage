#ifndef ETC_RAIDTAB_H
#define ETC_RAIDTAB_H

#include <string>
#include <map>

#include "y2storage/AsciiFile.h"


namespace storage
{

class EtcRaidtab
    {
    public:
	EtcRaidtab( const string& prefix="" );
	~EtcRaidtab();
	void updateEntry( unsigned num, const std::list<string>& entries,
	                  const string&, const std::list<string>& devs );
	void removeEntry( unsigned num );
    protected:
	struct entry
	    {
	    entry() : first(0), last(0) {}
	    entry(unsigned f, unsigned l) : first(f), last(l) {}
	    unsigned first;
	    unsigned last;
	    };

	void updateMdadmFile();
	void buildMdadmMap();

	int mdadm_dev_line;
	std::map<unsigned,entry> mtab;
	AsciiFile mdadm;
    };

}

#endif
