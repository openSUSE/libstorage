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
	bool GetInfo( const string& Dev, unsigned long long& SizeK, 
	              unsigned long& Major, unsigned long& Minor ) const;
	bool GetSize( const string& Dev, unsigned long long& SizeK ) const;
	list<string> GetMatchingEntries( const string& regexp ) const;
    protected:
	static string DevName( const string& Dev );
	map<string,int> co;
    };
///////////////////////////////////////////////////////////////////

#endif
