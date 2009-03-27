#ifndef PROC_PART_H
#define PROC_PART_H


#include <string>
#include <list>
#include <map>


namespace storage
{
    using std::string;
    using std::list;
    using std::map;


    class ProcPart
    {
    public:

	ProcPart();

	void reload();

	bool getSize(const string& device, unsigned long long& sizeK) const;

	list<string> getMatchingEntries(const string& regexp) const;

    protected:

	map<string, unsigned long long> data;

    };

}


#endif
