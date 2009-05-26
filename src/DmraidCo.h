#ifndef DMRAID_CO_H
#define DMRAID_CO_H

#include <list>

#include "storage/DmPartCo.h"
#include "storage/Dmraid.h"

namespace storage
{

class Storage;
class ProcPart;

class DmraidCo : public DmPartCo
    {
    friend class Storage;

    public:
	DmraidCo( Storage * const s, const string& Name, ProcPart& ppart );
	DmraidCo( Storage * const s, const string& Name, unsigned num, 
	          unsigned long long Size, ProcPart& ppart );
	DmraidCo( const DmraidCo& rhs );
	virtual ~DmraidCo();

	static storage::CType staticType() { return storage::DMRAID; }
	friend std::ostream& operator<< (std::ostream&, const DmraidCo& );
	void getInfo( storage::DmraidCoInfo& info ) const;
	void setUdevData(const list<string>& id);

	bool equalContent( const Container& rhs ) const;
	string getDiffString( const Container& d ) const;
	DmraidCo& operator= ( const DmraidCo& rhs );

    protected:

	// iterators over partitions
        // protected typedefs for iterators over partitions
        typedef CastIterator<VIter, Dmraid *> DmraidInter;
        typedef CastIterator<CVIter, const Dmraid *> DmraidCInter;
        template< class Pred >
            struct DmraidPI { typedef ContainerIter<Pred, DmraidInter> type; };
        template< class Pred >
            struct DmraidCPI { typedef ContainerIter<Pred, DmraidCInter> type; };
        typedef CheckFnc<const Dmraid> CheckFncDmraid;
        typedef CheckerIterator< CheckFncDmraid, DmraidPI<CheckFncDmraid>::type,
                                 DmraidInter, Dmraid > DmraidPIterator;
        typedef CheckerIterator< CheckFncDmraid, DmraidCPI<CheckFncDmraid>::type,
                                 DmraidCInter, const Dmraid > DmraidCPIterator;
	typedef DerefIterator<DmraidPIterator,Dmraid> DmraidIter;
	typedef DerefIterator<DmraidCPIterator,const Dmraid> ConstDmraidIter;
        typedef IterPair<DmraidIter> DmraidPair;
        typedef IterPair<ConstDmraidIter> ConstDmraidPair;

        DmraidPair dmraidPair( bool (* CheckDmraid)( const Dmraid& )=NULL)
            {
            return( DmraidPair( dmraidBegin( CheckDmraid ), dmraidEnd( CheckDmraid ) ));
            }
        DmraidIter dmraidBegin( bool (* CheckDmraid)( const Dmraid& )=NULL)
            {
	    IterPair<DmraidInter> p( (DmraidInter(begin())), (DmraidInter(end())) );
            return( DmraidIter( DmraidPIterator( p, CheckDmraid )) );
	    }
        DmraidIter dmraidEnd( bool (* CheckDmraid)( const Dmraid& )=NULL)
            {
	    IterPair<DmraidInter> p( (DmraidInter(begin())), (DmraidInter(end())) );
            return( DmraidIter( DmraidPIterator( p, CheckDmraid, true )) );
	    }

        ConstDmraidPair dmraidPair( bool (* CheckDmraid)( const Dmraid& )=NULL) const
            {
            return( ConstDmraidPair( dmraidBegin( CheckDmraid ), dmraidEnd( CheckDmraid ) ));
            }
        ConstDmraidIter dmraidBegin( bool (* CheckDmraid)( const Dmraid& )=NULL) const
            {
	    IterPair<DmraidCInter> p( (DmraidCInter(begin())), (DmraidCInter(end())) );
            return( ConstDmraidIter( DmraidCPIterator( p, CheckDmraid )) );
	    }
        ConstDmraidIter dmraidEnd( bool (* CheckDmraid)( const Dmraid& )=NULL) const
            {
	    IterPair<DmraidCInter> p( (DmraidCInter(begin())), (DmraidCInter(end())) );
            return( ConstDmraidIter( DmraidCPIterator( p, CheckDmraid, true )) );
	    }

	DmraidCo( Storage * const s, const string& File );
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new DmraidCo( *this ) ); }
	static void activate( bool val );
	void getRaidData( const string& name );
	void addRaid( const string& name );
	void addPv( Pv*& p );
	void newP( DmPart*& dm, unsigned num, Partition* p );
	string removeText( bool doing ) const;
	string setDiskLabelText( bool doing ) const;

	static string undevName( const string& name );

	static void getRaids( std::list<string>& l );
	static bool raidNotDeleted( const Dmraid&d ) { return( !d.deleted() ); }

	int doRemove();

	void logData(const string& Dir) const;

	string raidtype;
	string controller;

	static bool active;
    };

}

#endif
