#ifndef STORAGE_H
#define STORAGE_H

#include <iostream>
#include <list>

#include "y2storage/Container.h"
#include "y2storage/Disk.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/DerefIterator.h"
#include "y2storage/ListListIterator.h"
#include "y2storage/IterPair.h"

using namespace std;

class Storage
    {
    typedef list<Container*> CCont;
    typedef CCont::iterator CIter;
    typedef CCont::const_iterator CCIter;

    public:
	struct SkipDeleted { bool operator()(const Container&d) const {return( !d.Delete());}};
	static SkipDeleted SkipDel;
	static bool NotDeleted( const Container&d ) { return( !d.Delete() ); };
	static bool IsRealDisk( const Container&d ) 
	    { return( d.Type()==Container::DISK ); };

	Storage( bool ronly=false, bool autodetect=true );
	virtual ~Storage();
	int AddDisk( const string& Name );

	template< class Pred > 
	    struct CCP { typedef ContainerIter<Pred, CCIter> type; };
	template< class Pred > 
	    struct CC { typedef ContainerDerIter<Pred, typename CCP<Pred>::type, const Container> type; };
	template< class Pred >
	    struct ContCondIPair { typedef MakeCondIterPair<Pred, typename CC<Pred>::type> type;};

	typedef CastIterator<CCIter, Disk, const Disk *> ContainerCDiskIter;
	template< class Pred > 
	    struct CDP { typedef ContainerIter<Pred, ContainerCDiskIter> type; };
	template< class Pred > 
	    struct CD { typedef ContainerDerIter<Pred, typename CDP<Pred>::type, const Disk> type; };
	template< class Pred >
	    struct DiskCondIPair { typedef MakeCondIterPair<Pred, typename CD<Pred>::type> type; };

    protected:
	template< class Pred > 
	    struct CP { typedef ContainerIter<Pred, CIter> type; };
	template< class Pred > 
	    struct C { typedef ContainerDerIter<Pred, typename CP<Pred>::type, Container> type; };
	typedef CastIterator<CIter, Disk, Disk *> ContainerDiskIter;
	template< class Pred > 
	    struct DP { typedef ContainerIter<Pred, ContainerDiskIter> type; };
	template< class Pred > 
	    struct D { typedef ContainerDerIter<Pred, typename DP<Pred>::type, Disk> type; };

	typedef CheckFnc<const Container> CheckFncCont;
	typedef CheckFnc<const Volume> CheckFncVol;
	typedef CheckFnc<const Disk> CheckFncDisk;

    public:
	typedef CheckerIterator< CheckFncCont, CCP<CheckFncCont>::type, 
	                         CCIter, Container > ConstContIteratorP;
	typedef DerefIterator<ConstContIteratorP,const Container> ConstContIterator;
	typedef CheckerIterator< CheckFncDisk, CDP<CheckFncDisk>::type, 
	                         ContainerCDiskIter, Disk > ConstDiskIteratorP;
	typedef DerefIterator<ConstDiskIteratorP,const Disk> ConstDiskIterator;

	typedef ListListIterator<Container::ConstPlainIterator, ConstContIterator> ConstVolPart;
	template< class Pred > 
	    struct CVP { typedef ContainerIter<Pred, ConstVolPart> type; };
	template< class Pred > 
	    struct CV { typedef ContainerDerIter<Pred, typename CVP<Pred>::type, const Volume> type; };
	template< class Pred >
	    struct VolCondIPair { typedef MakeCondIterPair<Pred, typename CV<Pred>::type> type;};

	typedef CheckerIterator< CheckFncVol, CVP<CheckFncVol>::type, 
	                         ConstVolPart, Volume > ConstVolPIterator;
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;

    protected:
	typedef CheckerIterator< CheckFncCont, CP<CheckFncCont>::type, 
	                         CIter, Container > ContPIterator;

	typedef DerefIterator<ContPIterator,Container> ContIterator;

	typedef ListListIterator<Container::PlainIterator, ContIterator> VolPart;

	template< class Pred > 
	    struct V { typedef ContainerIter<Pred, VolPart> type; };

	typedef CheckerIterator< CheckFncVol, V<CheckFncVol>::type, 
	                         VolPart, Volume > VolIterator;

    public:
	typedef IterPair<ConstContIterator> ConstContPair;
	ConstContPair ContPair( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContPair( ContBegin( CheckFnc ), ContEnd( CheckFnc ) ));
	    }
	ConstContIterator ContBegin( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContIteratorP( Disks.begin(), Disks.end(), CheckFnc )) );
	    }
	ConstContIterator ContEnd( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContIteratorP( Disks.begin(), Disks.end(), CheckFnc, true )) );
	    }
	template< class Pred > typename ContCondIPair<Pred>::type ContCondPair( const Pred& p ) const
	    {
	    return( ContCondIPair<Pred>::type( ContCondBegin( p ), ContCondEnd( p ) ) );
	    }
	template< class Pred > typename CC<Pred>::type ContCondBegin( const Pred& p ) const
	    {
	    return( CC<Pred>::type( CCP<Pred>::type( Disks.begin(), Disks.end(), p )) );
	    }
	template< class Pred > typename CC<Pred>::type ContCondEnd( const Pred& p ) const
	    {
	    return( CC<Pred>::type( CCP<Pred>::type( Disks.begin(), Disks.end(), p, true )) );
	    }

	typedef IterPair<ConstDiskIterator> ConstDiskPair;
	ConstDiskPair DiskPair( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    return( ConstDiskPair( DiskBegin( CheckFnc ), DiskEnd( CheckFnc ) ));
	    }
	ConstDiskIterator DiskBegin( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( Disks.begin(), Disks.end() ),
	                                    ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( ConstDiskIterator( ConstDiskIteratorP( p, CheckFnc )) );
	    }
	ConstDiskIterator DiskEnd( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( Disks.begin(), Disks.end() ),
	                                    ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( ConstDiskIterator( ConstDiskIteratorP( p, CheckFnc, true )) );
	    }
	template< class Pred > typename DiskCondIPair<Pred>::type DiskCondPair( const Pred& p ) const
	    {
	    return( DiskCondIPair<Pred>::type( DiskCondBegin( p ), DiskCondEnd( p ) ) );
	    }
	template< class Pred > typename CD<Pred>::type DiskCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( Disks.begin(), Disks.end() ),
					       ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( CD<Pred>::type( CDP<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename CD<Pred>::type DiskCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( Disks.begin(), Disks.end() ),
					       ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( CD<Pred>::type( CDP<Pred>::type( pair, p, true )) );
	    }

	typedef IterPair<ConstVolIterator> ConstVolPair;
	ConstVolPair VolPair( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( ConstVolPair( VolBegin( CheckCnt ), VolEnd( CheckCnt ) ));
	    }
	ConstVolPair VolPair( bool (* CheckVol)( const Volume& )=NULL,
			      bool (* CheckCnt)( const Container& )=NULL) const
	    { 
	    return( ConstVolPair( VolBegin( CheckVol, CheckCnt ),
	                          VolEnd( CheckVol, CheckCnt ) ));
	    }
	ConstVolIterator VolBegin( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( VolBegin( NULL, CheckCnt ) );
	    }
	ConstVolIterator VolBegin( bool (* CheckVol)( const Volume& )=NULL, 
	                           bool (* CheckCnt)( const Container& )=NULL) const
	    {
	    IterPair<ConstVolPart> p( *new ConstVolPart( ContPair( CheckCnt )),
	                              *new ConstVolPart( ContPair( CheckCnt ), true ));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol )));
	    }
	ConstVolIterator VolEnd( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( VolEnd( NULL, CheckCnt ) );
	    }
	ConstVolIterator VolEnd( bool (* CheckVol)( const Volume& )=NULL, 
	                         bool (* CheckCnt)( const Container& )=NULL) const
	    { 
	    IterPair<ConstVolPart> p( *new ConstVolPart( ContPair( CheckCnt )),
	                              *new ConstVolPart( ContPair( CheckCnt ), true ));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol, true )));
	    }
	template< class Pred > typename VolCondIPair<Pred>::type VolCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>::type( VolCondBegin( p ), VolCondEnd( p ) ) );
	    }
	template< class Pred > typename CV<Pred>::type VolCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstVolPart> pair( *new ConstVolPart( ContPair()),
	                                 *new ConstVolPart( ContPair(), true ));
	    return( CV<Pred>::type( CVP<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename CV<Pred>::type VolCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstVolPart> pair( *new ConstVolPart( ContPair()),
	                                 *new ConstVolPart( ContPair(), true ));
	    return( CV<Pred>::type( CVP<Pred>::type(pair, p, true )) );
	    }


    protected:
	typedef IterPair<ContIterator> CIPair;
	CIPair CPair( bool (* CheckFnc)( const Container& )=NULL )
	    {
	    return( CIPair( CBegin(CheckFnc), CEnd(CheckFnc) ));
	    }
	ContIterator CBegin( bool (* CheckFnc)( const Container& )=NULL )
	    { 
	    return( ContIterator( ContPIterator( Disks.begin(), Disks.end(), CheckFnc )) );
	    }
	ContIterator CEnd( bool (* CheckFnc)( const Container& )=NULL )
	    { 
	    return( ContIterator( ContPIterator( Disks.begin(), Disks.end(), CheckFnc, true )) );
	    }

	typedef IterPair<VolIterator> VIPair;
	VIPair VPair( bool (* CheckCnt)( const Container& )) 
	    { 
	    return( VIPair( VBegin( CheckCnt ), VEnd( CheckCnt ) ));
	    }
	VIPair VPair( bool (* CheckVol)( const Volume& )=NULL,
		      bool (* CheckCnt)( const Container& )=NULL) 
	    { 
	    return( VIPair( VBegin( CheckVol, CheckCnt ),
			    VEnd( CheckVol, CheckCnt ) ));
	    }
	VolIterator VBegin( bool (* CheckCnt)( const Container& )) 
	    { 
	    return( VBegin( NULL, CheckCnt ) );
	    }
	VolIterator VBegin( bool (* CheckVol)( const Volume& )=NULL, 
			    bool (* CheckCnt)( const Container& )=NULL) 
	    {
	    IterPair<VolPart> p( *new VolPart( CPair( CheckCnt )),
				 *new VolPart( CPair( CheckCnt ), true ));
	    return( VolIterator( p, CheckVol ));
	    }
	VolIterator VEnd( bool (* CheckCnt)( const Container& ))
	    { 
	    return( VEnd( NULL, CheckCnt ) );
	    }
	VolIterator VEnd( bool (* CheckVol)( const Volume& )=NULL, 
			  bool (* CheckCnt)( const Container& )=NULL)
	    { 
	    IterPair<VolPart> p( *new VolPart( CPair( CheckCnt )),
				 *new VolPart( CPair( CheckCnt ), true ));
	    return( VolIterator( p, CheckVol, true ));
	    }

	void AutodetectDisks();
	bool readonly;
	CCont Disks;
    };

Storage::SkipDeleted Storage::SkipDel;


#endif
