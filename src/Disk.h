#ifndef DISK_H
#define DISK_H

#include <list>

using namespace std;

#include "y2storage/Container.h"
#include "y2storage/Partition.h"

class Storage;
class SystemCmd;
class ProcPart;

class Disk : public Container
    {
    friend class Storage;

    struct label_info 
	{
	string name;
	bool extended;
	unsigned primary;
	unsigned logical;
	};

    public:
	Disk( const Storage * const s, const string& Name, unsigned long long Size );
	virtual ~Disk();
	unsigned cylinders() const { return cyl; }
	unsigned heads() const { return head; }
	unsigned sectors() const { return sector; }
	unsigned long long sizeK() const { return size_k; }
	unsigned long minor() const { return mnr; }
	unsigned long major() const { return mjr; }
	unsigned long numMinor() const { return range; }
	static CType const staticType() { return DISK; }
	static bool needP( const string& dev );
	friend inline ostream& operator<< (ostream&, const Disk& );

    protected:
	Disk( const Storage * const s, const string& File );
	unsigned long long cylinderToKb( unsigned long );
	unsigned long kbToCylinder( unsigned long long );
	unsigned long long capacityInKb();
	bool detectGeometry();
	bool detectPartitions();
	bool getSysfsInfo( const string& SysFsDir );
	void checkSystemError( const string& cmd_line, const SystemCmd& cmd );
	bool checkPartedOutput( const SystemCmd& cmd );
	bool scanPartedLine( const string& Line, unsigned& nr, 
	                     unsigned long& start, unsigned long& csize, 
			     Partition::PType& type, string& parted_start, 
			     unsigned& id, bool& boot );
	bool checkPartedValid( const ProcPart& pp, const list<string>& ps,
	                       const list<Partition*>& pl );
	void logData( const string& Dir );
	bool haveBsdPart() const;
	void setLabelData( const string& );

	static string defaultLabel();
	static label_info labels[];
	static string p_disks[];
	static string getPartName( const string& disk, unsigned nr );
	static string getPartName( const string& disk, const string& nr );
	static pair<string,long> getDiskPartition( const string& dev );

	unsigned long cyl;
	unsigned head;
	unsigned sector;
	string label;
	string system_stderr;
	int max_primary;
	bool ext_possible;
	int max_logical;
	unsigned long byte_cyl;
	unsigned long long size_k;
	unsigned long mnr;
	unsigned long mjr;
	unsigned long range;
    };

inline ostream& operator<< (ostream& s, const Disk& d )
    {
    d.print(s);
    s << " Cyl:" << d.cyl
      << " Head:" << d.head
      << " Sect:" << d.sector
      << " Node <" << d.mjr
      << ":" << d.mnr << ">"
      << " Range:" << d.range
      << " SizeM:" << d.size_k/1024
      << " Label:" << d.label
      << " MaxPrimary:" << d.max_primary;
    if( d.ext_possible )
	s << " ExtPossible MaxLogical:" << d.max_logical;
    return( s );
    }


#endif
