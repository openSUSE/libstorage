#ifndef ETC_RAIDTAB_H
#define ETC_RAIDTAB_H

#include <string>
#include <map>

#include "y2storage/AsciiFile.h"

class Regex;

class EtcRaidtab : public AsciiFile
    {
    public:
	EtcRaidtab( const string& prefix="" );
	~EtcRaidtab();
	void updateEntry( unsigned num, const list<string>& entries );
	void removeEntry( unsigned num );
    protected:
	struct entry
	    {
	    entry() { first=last=0; }
	    entry( unsigned f, unsigned l ) { first=f; last=l; }
	    unsigned first;
	    unsigned last;
	    friend ostream& operator<< (ostream& s, const entry &v );
	    };
	friend ostream& operator<< (ostream& s, const entry &v );

	void updateFile();
	void buildMap();

	Regex *whitespace;
	Regex *comment;
	string filename;
	map<unsigned,entry> co;
    };
///////////////////////////////////////////////////////////////////

inline ostream& operator<< (ostream& s, const EtcRaidtab::entry& v )
    {
    s << "first=" << v.first << " last=" << v.last;
    return( s );
    }

#endif
