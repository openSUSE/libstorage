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
	    struct cCP { typedef ContainerIter<Pred, CCIter> type; };
	template< class Pred > 
	    struct cC { typedef ContainerDerIter<Pred, typename cCP<Pred>::type, const Container> type; };
	template< class Pred >
	    struct ContCondIPair { typedef MakeCondIterPair<Pred, typename cC<Pred>::type> type;};

	typedef CastCheckIterator<CCIter, Disk, const Disk *> ContainerCDiskIter;
	template< class Pred > 
	    struct cDP { typedef ContainerIter<Pred, ContainerCDiskIter> type; };
	template< class Pred > 
	    struct cD { typedef ContainerDerIter<Pred, typename cDP<Pred>::type, const Disk> type; };
	template< class Pred >
	    struct DiskCondIPair { typedef MakeCondIterPair<Pred, typename cD<Pred>::type> type; };

    protected:
	template< class Pred > 
	    struct CP { typedef ContainerIter<Pred, CIter> type; };
	template< class Pred > 
	    struct C { typedef ContainerDerIter<Pred, typename CP<Pred>::type, Container> type; };
	typedef CastCheckIterator<CIter, Disk, Disk *> ContainerDiskIter;
	template< class Pred > 
	    struct DP { typedef ContainerIter<Pred, ContainerDiskIter> type; };
	template< class Pred > 
	    struct D { typedef ContainerDerIter<Pred, typename DP<Pred>::type, Disk> type; };

	typedef CheckFnc<const Container> CheckFncCont;
	typedef CheckFnc<const Volume> CheckFncVol;
	typedef CheckFnc<const Disk> CheckFncDisk;
	typedef CheckFnc<const Partition> CheckFncPartition;

    public:
	typedef CheckerIterator< CheckFncCont, cCP<CheckFncCont>::type, 
	                         CCIter, Container > ConstContPIterator;
	typedef DerefIterator<ConstContPIterator,const Container> ConstContIterator;
	typedef CheckerIterator< CheckFncDisk, cDP<CheckFncDisk>::type, 
	                         ContainerCDiskIter, Disk > ConstDiskPIterator;
	typedef DerefIterator<ConstDiskPIterator,const Disk> ConstDiskIterator;

	typedef ListListIterator<Container::ConstPlainIterator, ConstContIterator> ConstVolInter;
	template< class Pred > 
	    struct cVP { typedef ContainerIter<Pred, ConstVolInter> type; };
	template< class Pred > 
	    struct cV { typedef ContainerDerIter<Pred, typename cVP<Pred>::type, const Volume> type; };
	template< class Pred >
	    struct VolCondIPair { typedef MakeCondIterPair<Pred, typename cV<Pred>::type> type;};

	typedef CheckerIterator< CheckFncVol, cVP<CheckFncVol>::type, 
	                         ConstVolInter, Volume > ConstVolPIterator;
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;

	typedef ListListIterator<Container::ConstPlainIterator, ConstDiskIterator> ConstPartInter;
	typedef CastIterator<ConstPartInter, Partition *> ConstPartInter2;
	template< class Pred > 
	    struct cPP { typedef ContainerIter<Pred, ConstPartInter2> type; };
	template< class Pred > 
	    struct cP { typedef ContainerDerIter<Pred, typename cPP<Pred>::type, const Partition> type; };
	template< class Pred >
	    struct PartCondIPair { typedef MakeCondIterPair<Pred, typename cP<Pred>::type> type;};

	typedef CheckerIterator< CheckFncPartition, cPP<CheckFncPartition>::type, 
	                         ConstPartInter2, Partition > ConstPartPIterator;
	typedef DerefIterator<ConstPartPIterator, const Partition> ConstPartIterator;

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
	// various public iterators over container detected in the system
	typedef IterPair<ConstContIterator> ConstContPair;
	ConstContPair ContPair( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContPair( ContBegin( CheckFnc ), ContEnd( CheckFnc ) ));
	    }
	ConstContIterator ContBegin( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContPIterator( Disks.begin(), Disks.end(), CheckFnc )) );
	    }
	ConstContIterator ContEnd( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContPIterator( Disks.begin(), Disks.end(), CheckFnc, true )) );
	    }
	template< class Pred > typename ContCondIPair<Pred>::type ContCondPair( const Pred& p ) const
	    {
	    return( ContCondIPair<Pred>::type( ContCondBegin( p ), ContCondEnd( p ) ) );
	    }
	template< class Pred > typename cC<Pred>::type ContCondBegin( const Pred& p ) const
	    {
	    return( cC<Pred>::type( cCP<Pred>::type( Disks.begin(), Disks.end(), p )) );
	    }
	template< class Pred > typename cC<Pred>::type ContCondEnd( const Pred& p ) const
	    {
	    return( cC<Pred>::type( cCP<Pred>::type( Disks.begin(), Disks.end(), p, true )) );
	    }

	// various public iterators over disks detected in the system
	typedef IterPair<ConstDiskIterator> ConstDiskPair;
	ConstDiskPair DiskPair( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    return( ConstDiskPair( DiskBegin( CheckFnc ), DiskEnd( CheckFnc ) ));
	    }
	ConstDiskIterator DiskBegin( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( Disks.begin(), Disks.end() ),
	                                    ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( ConstDiskIterator( ConstDiskPIterator( p, CheckFnc )) );
	    }
	ConstDiskIterator DiskEnd( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( Disks.begin(), Disks.end() ),
	                                    ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( ConstDiskIterator( ConstDiskPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename DiskCondIPair<Pred>::type DiskCondPair( const Pred& p ) const
	    {
	    return( DiskCondIPair<Pred>::type( DiskCondBegin( p ), DiskCondEnd( p ) ) );
	    }
	template< class Pred > typename cD<Pred>::type DiskCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( Disks.begin(), Disks.end() ),
					       ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( cD<Pred>::type( cDP<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename cD<Pred>::type DiskCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( Disks.begin(), Disks.end() ),
					       ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( cD<Pred>::type( cDP<Pred>::type( pair, p, true )) );
	    }

	// various public iterators over volumes detected in the system
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
	    IterPair<ConstVolInter> p = IterPair<ConstVolInter>(ConstVolInter( ContPair( CheckCnt )),
							        ConstVolInter( ContPair( CheckCnt ), true ));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol )));
	    }
	ConstVolIterator VolEnd( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( VolEnd( NULL, CheckCnt ) );
	    }
	ConstVolIterator VolEnd( bool (* CheckVol)( const Volume& )=NULL, 
	                         bool (* CheckCnt)( const Container& )=NULL) const
	    { 
	    IterPair<ConstVolInter> p = IterPair<ConstVolInter>( ConstVolInter( ContPair( CheckCnt )),
							         ConstVolInter( ContPair( CheckCnt ), true ));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol, true )));
	    }
	template< class Pred > typename VolCondIPair<Pred>::type VolCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>::type( VolCondBegin( p ), VolCondEnd( p ) ) );
	    }
	template< class Pred > typename cV<Pred>::type VolCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstVolInter> pair = IterPair<ConstVolInter>( ConstVolInter( ContPair()),
								    ConstVolInter( ContPair(), true ));
	    return( cV<Pred>::type( cVP<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename cV<Pred>::type VolCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstVolInter> pair = IterPair<ConstVolInter>( ConstVolInter( ContPair()),
								    ConstVolInter( ContPair(), true ));
	    return( cV<Pred>::type( cVP<Pred>::type(pair, p, true )) );
	    }

	// various public iterators over partitions detected in the system
	typedef IterPair<ConstPartIterator> ConstPartPair;
	ConstPartPair PartPair( bool (* CheckCnt)( const Disk& )) const
	    { 
	    return( ConstPartPair( PartBegin( CheckCnt ), PartEnd( CheckCnt ) ));
	    }
	ConstPartPair PartPair( bool (* CheckPart)( const Partition& )=NULL,
				bool (* CheckCnt)( const Disk& )=NULL) const
	    { 
	    return( ConstPartPair( PartBegin( CheckPart, CheckCnt ),
	                           PartEnd( CheckPart, CheckCnt ) ));
	    }
	ConstPartIterator PartBegin( bool (* CheckDisk)( const Disk& )) const
	    { 
	    return( PartBegin( NULL, CheckDisk ) );
	    }
	ConstPartIterator PartBegin( bool (* CheckPart)( const Partition& )=NULL, 
	                             bool (* CheckDisk)( const Disk& )=NULL) const
	    {
	    IterPair<ConstPartInter2> p = 
		IterPair<ConstPartInter2>(ConstPartInter2( ConstPartInter(DiskPair( CheckDisk ))),
					  ConstPartInter2( ConstPartInter(DiskPair( CheckDisk ), true )));
	    return( ConstPartIterator( ConstPartPIterator(p, CheckPart )));
	    }
	ConstPartIterator PartEnd( bool (* CheckDisk)( const Disk& )) const
	    { 
	    return( PartEnd( NULL, CheckDisk ) );
	    }
	ConstPartIterator PartEnd( bool (* CheckPart)( const Partition& )=NULL, 
	                           bool (* CheckDisk)( const Disk& )=NULL) const
	    { 
	    IterPair<ConstPartInter2> p = 
		IterPair<ConstPartInter2>(ConstPartInter2( ConstPartInter(DiskPair( CheckDisk ))),
					  ConstPartInter2( ConstPartInter(DiskPair( CheckDisk ), true )));
	    return( ConstPartIterator( ConstPartPIterator(p, CheckPart, true )));
	    }
	template< class Pred > typename PartCondIPair<Pred>::type PartCondPair( const Pred& p ) const
	    {
	    return( PartCondIPair<Pred>::type( PartCondBegin( p ), PartCondEnd( p ) ) );
	    }
	template< class Pred > typename cP<Pred>::type PartCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstPartInter2> pair = IterPair<ConstPartInter2>( ConstPartInter( ContPair()),
								        ConstPartInter( ContPair(), true ));
	    return( cP<Pred>::type( cPP<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename cP<Pred>::type PartCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstPartInter2> pair = IterPair<ConstPartInter2>( ConstPartInter( ContPair()),
								        ConstPartInter( ContPair(), true ));
	    return( cP<Pred>::type( cPP<Pred>::type(pair, p, true )) );
	    }


    protected:
	// various protected iterators over containers detected in the system
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

	// various protected iterators over volumes detected in the system
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
	    IterPair<VolPart> p = IterPair<VolPart>( VolPart( CPair( CheckCnt )),
						     VolPart( CPair( CheckCnt ), true ));
	    return( VolIterator( p, CheckVol ));
	    }
	VolIterator VEnd( bool (* CheckCnt)( const Container& ))
	    { 
	    return( VEnd( NULL, CheckCnt ) );
	    }
	VolIterator VEnd( bool (* CheckVol)( const Volume& )=NULL, 
			  bool (* CheckCnt)( const Container& )=NULL)
	    { 
	    IterPair<VolPart> p = IterPair<VolPart>( VolPart( CPair( CheckCnt )),
						     VolPart( CPair( CheckCnt ), true ));
	    return( VolIterator( p, CheckVol, true ));
	    }

	void AutodetectDisks();
	bool readonly;
	CCont Disks;
    };

Storage::SkipDeleted Storage::SkipDel;


#endif
