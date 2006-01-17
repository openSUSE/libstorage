#ifndef PARTITION_H
#define PARTITION_H

#include "y2storage/StorageInterface.h"
#include "y2storage/Volume.h"
#include "y2storage/Region.h"

namespace storage
{

class Disk;

class Partition : public Volume
    {
    public:

	typedef enum { ID_DOS16=0x6, ID_DOS=0x0c, ID_NTFS=0x07,
	               ID_EXTENDED=0x0f,
		       ID_LINUX=0x83, ID_SWAP=0x82, ID_LVM=0x8e, ID_RAID=0xfd,
		       ID_APPLE_OTHER=0x101, ID_APPLE_HFS=0x102,
		       ID_GPT_BOOT=0x103, ID_GPT_SERVICE=0x104,
		       ID_GPT_MSFTRES=0x105 } IdNum;

	Partition( const Disk& d, unsigned Pnr, unsigned long long SizeK,
	           unsigned long Start, unsigned long CSize,
		   storage::PartitionType Type, 
		   unsigned id=ID_LINUX, bool Boot=false );
	Partition( const Disk& d, const string& Data );
	Partition( const Disk& d, const Partition& d );
	virtual ~Partition();

	unsigned long cylStart() const { return reg.start(); }
	unsigned long cylSize() const { return reg.len(); }
	unsigned long cylEnd() const { return reg.start()+reg.len()-1; }
	const Region& region() const { return reg; }
	const string& udevId() const;
	const string& udevPath() const;
	bool intersectArea( const Region& r, unsigned fuzz=0 ) const;
	bool contains( const Region& r, unsigned fuzz=0 ) const;
	unsigned OrigNr() const { return( orig_num ); }
	bool boot() const { return bootflag; }
	unsigned id() const { return idt; }
	storage::PartitionType type() const { return typ; }
	std::ostream& logData( std::ostream& file ) const;
	void changeRegion( unsigned long Start, unsigned long CSize,
	                   unsigned long long SizeK );
	void changeNumber( unsigned new_num );
	void changeId( unsigned id );
	void changeIdDone();
	void unChangeId();
	string removeText( bool doing=true ) const;
	string createText( bool doing=true ) const;
	string formatText(bool doing=true) const;
	string resizeText(bool doing=true) const;
	void getCommitActions( std::list<storage::commitAction*>& l ) const;
	string setTypeText( bool doing=true ) const;
	int setFormat( bool format=true, storage::FsType fs=storage::REISERFS );
	int changeMount( const string& val );
	const Disk* const disk() const;
	bool isWindows() const;
	friend std::ostream& operator<< (std::ostream& s, const Partition &p );
	static bool notDeleted( const Partition&d ) { return( !d.deleted() ); }
	static bool toChangeId( const Partition&d ) { return( !d.deleted() && d.idt!=d.orig_id ); }
	virtual void print( std::ostream& s ) const { s << *this; }
	void setResizedSize( unsigned long long SizeK );
	void forgetResize(); 
	bool canUseDevice() const;

	void getInfo( storage::PartitionInfo& info ) const;
	bool equalContent( const Partition& rhs ) const;
	void logDifference( const Partition& d ) const;

    protected:
	Partition& operator=( const Partition& );

	Region reg;
	bool bootflag;
	storage::PartitionType typ;
	unsigned idt;
	unsigned orig_id;
	string parted_start;
	unsigned orig_num;

	static string pt_names[storage::PTYPE_ANY+1];
	mutable storage::PartitionInfo info;
    };

}

#endif
