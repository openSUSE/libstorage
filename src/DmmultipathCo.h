#ifndef DMMULTIPATH_CO_H
#define DMMULTIPATH_CO_H

#include <list>

#include "storage/DmPartCo.h"
#include "storage/Dmmultipath.h"

namespace storage
{

class Storage;
class ProcPart;

class DmmultipathCo : public DmPartCo
    {
    friend class Storage;

    public:
	DmmultipathCo( Storage * const s, const string& Name, ProcPart& ppart );
	DmmultipathCo( Storage * const s, const string& Name, unsigned num,
		       unsigned long long Size, ProcPart& ppart );
	DmmultipathCo( const DmmultipathCo& rhs );
	virtual ~DmmultipathCo();

	static storage::CType staticType() { return storage::DMMULTIPATH; }
	friend std::ostream& operator<< (std::ostream&, const DmmultipathCo& );
	void getInfo( storage::DmmultipathCoInfo& info ) const;
	void setUdevData(const list<string>& id);

	bool equalContent( const Container& rhs ) const;
	string getDiffString( const Container& d ) const;
	DmmultipathCo& operator= ( const DmmultipathCo& rhs );

    protected:

	// iterators over partitions
        // protected typedefs for iterators over partitions
        typedef CastIterator<VIter, Dmmultipath *> DmmultipathInter;
        typedef CastIterator<CVIter, const Dmmultipath *> DmmultipathCInter;
        template< class Pred >
            struct DmmultipathPI { typedef ContainerIter<Pred, DmmultipathInter> type; };
        template< class Pred >
            struct DmmultipathCPI { typedef ContainerIter<Pred, DmmultipathCInter> type; };
        typedef CheckFnc<const Dmmultipath> CheckFncDmmultipath;
        typedef CheckerIterator< CheckFncDmmultipath, DmmultipathPI<CheckFncDmmultipath>::type,
                                 DmmultipathInter, Dmmultipath > DmmultipathPIterator;
        typedef CheckerIterator< CheckFncDmmultipath, DmmultipathCPI<CheckFncDmmultipath>::type,
                                 DmmultipathCInter, const Dmmultipath > DmmultipathCPIterator;
	typedef DerefIterator<DmmultipathPIterator,Dmmultipath> DmmultipathIter;
	typedef DerefIterator<DmmultipathCPIterator,const Dmmultipath> ConstDmmultipathIter;
        typedef IterPair<DmmultipathIter> DmmultipathPair;
        typedef IterPair<ConstDmmultipathIter> ConstDmmultipathPair;

        DmmultipathPair dmmultipathPair( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL)
            {
            return( DmmultipathPair( dmmultipathBegin( CheckDmmultipath ), dmmultipathEnd( CheckDmmultipath ) ));
            }
        DmmultipathIter dmmultipathBegin( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL)
            {
	    IterPair<DmmultipathInter> p( (DmmultipathInter(begin())), (DmmultipathInter(end())) );
            return( DmmultipathIter( DmmultipathPIterator( p, CheckDmmultipath )) );
	    }
        DmmultipathIter dmmultipathEnd( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL)
            {
	    IterPair<DmmultipathInter> p( (DmmultipathInter(begin())), (DmmultipathInter(end())) );
            return( DmmultipathIter( DmmultipathPIterator( p, CheckDmmultipath, true )) );
	    }

        ConstDmmultipathPair dmmultipathPair( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL) const
            {
            return( ConstDmmultipathPair( dmmultipathBegin( CheckDmmultipath ), dmmultipathEnd( CheckDmmultipath ) ));
            }
        ConstDmmultipathIter dmmultipathBegin( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL) const
            {
	    IterPair<DmmultipathCInter> p( (DmmultipathCInter(begin())), (DmmultipathCInter(end())) );
            return( ConstDmmultipathIter( DmmultipathCPIterator( p, CheckDmmultipath )) );
	    }
        ConstDmmultipathIter dmmultipathEnd( bool (* CheckDmmultipath)( const Dmmultipath& )=NULL) const
            {
	    IterPair<DmmultipathCInter> p( (DmmultipathCInter(begin())), (DmmultipathCInter(end())) );
            return( ConstDmmultipathIter( DmmultipathCPIterator( p, CheckDmmultipath, true )) );
	    }

	DmmultipathCo( Storage * const s, const string& File );
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const { return( new DmmultipathCo( *this ) ); }
	void getMultipathData( const string& name );
	void addMultipath( const string& name );
	void addPv( Pv*& p );
	void newP( DmPart*& dm, unsigned num, Partition* p );
	string setDiskLabelText( bool doing ) const;

	static string undevName( const string& name );

	static void activate( bool val=true );
	static bool isActive() { return active; }

	static void getMultipaths( std::list<string>& l );
	static bool multipathNotDeleted( const Dmmultipath&d ) { return( !d.deleted() ); }

	void logData(const string& Dir) const;

	string vendor;
	string model;

	static bool active;
    };

}

#endif
