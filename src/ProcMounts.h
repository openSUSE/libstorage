#ifndef PROC_MOUNTS_H
#define PROC_MOUNTS_H

#include <string>
#include <list>
#include <map>

class ProcMounts 
    {
    public:
	ProcMounts();
	string getMount( const string& Dev ) const;
	string getMount( const list<string>& dl ) const;
	map<string,string> allMounts() const;
    protected:
	map<string,string> co;
    };
///////////////////////////////////////////////////////////////////

#endif
