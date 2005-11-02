#ifndef PROC_MOUNTS_H
#define PROC_MOUNTS_H

#include <string>
#include <list>
#include <map>

namespace storage
{

class ProcMounts 
    {
    public:
	ProcMounts();
	string getMount( const string& Dev ) const;
	string getMount( const std::list<string>& dl ) const;
	std::map<string,string> allMounts() const;
    protected:
	std::map<string,string> co;
    };

}

#endif
