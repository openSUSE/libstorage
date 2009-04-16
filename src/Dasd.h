#ifndef DASD_H
#define DASD_H

#include "y2storage/Disk.h"


namespace storage
{
    using std::list;


class SystemCmd;
class ProcPart;

class Dasd : public Disk
    {
    friend class Storage;
    public:
	Dasd( Storage * const s, const string& Name, unsigned long long Size );
	Dasd( const Dasd& rhs );
	virtual ~Dasd();
        int createPartition( storage::PartitionType type, long unsigned start,
	                     long unsigned len, string& device,
			     bool checkRelaxed=false );
        int removePartition( unsigned nr );
        int changePartitionId( unsigned nr, unsigned id ) { return 0; }
        int resizePartition( Partition* p, unsigned long newCyl );
	int initializeDisk( bool value );
	string fdasdText() const;
	string dasdfmtText( bool doing ) const;
	static string dasdfmtTexts( bool single, const string& devs );
	void getCommitActions(list<commitAction>& l) const;
	void getToCommit(storage::CommitStage stage, list<const Container*>& col,
			 list<const Volume*>& vol) const;
	int commitChanges( storage::CommitStage stage );
	bool detectGeometry();

    protected:
	enum DasdFormat { DASDF_NONE, DASDF_LDL, DASDF_CDL };

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new Dasd( *this ) ); }
	bool detectPartitionsFdasd(ProcPart& ppart);
	bool detectPartitions( ProcPart& ppart );
	bool checkFdasdOutput( SystemCmd& Cmd, ProcPart& ppart );
	bool scanFdasdLine( const string& Line, unsigned& nr, 
	                    unsigned long& start, unsigned long& csize );
	void getGeometry( SystemCmd& cmd, unsigned long& c,
			  unsigned& h, unsigned& s );
	void redetectGeometry() {};
        int doCreate( Volume* v ) { return(doFdasd()); }
        int doRemove( Volume* v ) { return(init_disk?0:doFdasd()); }
	int doFdasd();
        int doResize( Volume* v );
        int doSetType( Volume* v ) { return 0; }
        int doCreateLabel() { return 0; }
	int doDasdfmt();
	DasdFormat fmt;

	Dasd& operator= ( const Dasd& rhs );
	friend std::ostream& operator<< (std::ostream&, const Dasd& );

    };

}

#endif
