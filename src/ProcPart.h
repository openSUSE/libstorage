#ifndef PROC_PART_H
#define PROC_PART_H

#include <string>
#include <list>
#include <map>

#include "y2storage/AsciiFile.h"

class ProcPart : public AsciiFile
    {
    public:
	ProcPart();
	bool getInfo( const string& Dev, unsigned long long& SizeK, 
	              unsigned long& Major, unsigned long& Minor ) const;
	bool getSize( const string& Dev, unsigned long long& SizeK ) const;
	list<string> getMatchingEntries( const string& regexp ) const;
    protected:
	static string devName( const string& Dev );
	map<string,int> co;
    };
///////////////////////////////////////////////////////////////////

#endif
