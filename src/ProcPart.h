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

	list<string> getEntries() const;

	template<class Pred>
	list<string> getMatchingEntries(Pred pred) const
	{
	    list<string> ret;
	    for (const_iterator i = data.begin(); i != data.end(); ++i)
		if (pred(i->first))
		    ret.push_back(i->first);
	    return ret;
	}

    protected:

	typedef map<string, unsigned long long>::const_iterator const_iterator;

	map<string, unsigned long long> data;

    };

}


#endif
