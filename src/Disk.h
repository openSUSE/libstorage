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
	unsigned Cylinders() const { return cylinders; }
	unsigned Heads() const { return heads; }
	unsigned Sectors() const { return sectors; }
	unsigned long long SizeK() const { return size_k; }
	unsigned long Minor() const { return minor; }
	unsigned long Major() const { return major; }
	unsigned long NumMinor() const { return range; }
	static CType const StaticType() { return DISK; }
	friend inline ostream& operator<< (ostream&, const Disk& );

    protected:
	Disk( const Storage * const s, const string& File );
	unsigned long long CylinderToKb( unsigned long );
	unsigned long KbToCylinder( unsigned long long );
	unsigned long long CapacityInKb();
	bool DetectGeometry();
	bool DetectPartitions();
	bool GetSysfsInfo( const string& SysFsDir );
	void CheckSystemError( const string& cmd_line, const SystemCmd& cmd );
	bool CheckPartedOutput( const SystemCmd& cmd );
	bool ScanPartedLine( const string& Line, unsigned& nr, 
	                     unsigned long& start, unsigned long& csize, 
			     Partition::PType& type, string& parted_start, 
			     unsigned& id, bool& boot );
	bool CheckPartedValid( const ProcPart& pp, const list<string>& ps,
	                       const list<Partition*>& pl );
	void LogData( const string& Dir );
	bool HaveBsdPart() const;
	void SetLabelData( const string& );

	static string DefaultLabel();
	static label_info labels[];
	static string p_disks[];
	static bool NeedP( const string& dev );
	static string GetPartName( const string& disk, unsigned nr );
	static string GetPartName( const string& disk, const string& nr );
	static pair<string,long> GetDiskPartition( const string& dev );

	unsigned long cylinders;
	unsigned heads;
	unsigned sectors;
	string label;
	string system_stderr;
	int max_primary;
	bool ext_possible;
	int max_logical;
	unsigned long byte_cyl;
	unsigned long long size_k;
	unsigned long minor;
	unsigned long major;
	unsigned long range;
    };

inline ostream& operator<< (ostream& s, const Disk& d )
    {
    d.print(s);
    s << " Cyl:" << d.cylinders
      << " Head:" << d.heads
      << " Sect:" << d.sectors
      << " Node <" << d.major
      << ":" << d.minor << ">"
      << " Range:" << d.range
      << " SizeM:" << d.size_k/1024
      << " Label:" << d.label
      << " MaxPrimary:" << d.max_primary;
    if( d.ext_possible )
	s << " ExtPossible MaxLogical:" << d.max_logical;
    return( s );
    }


#endif
