#ifndef STORAGE_H
#define STORAGE_H

#include <iostream>
#include <list>

#include "y2storage/Container.h"
#include "y2storage/Disk.h"
#include "y2storage/Partition.h"
#include "y2storage/LvmVg.h"
#include "y2storage/LvmLv.h"
#include "y2storage/Evms.h"
#include "y2storage/EvmsVol.h"
#include "y2storage/Md.h"
#include "y2storage/Loop.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/FilterIterator.h"
#include "y2storage/DerefIterator.h"
#include "y2storage/ListListIterator.h"
#include "y2storage/IterPair.h"

using namespace std;

class Storage
    {
    protected:
	typedef list<Container*> CCont;
	typedef CCont::iterator CIter;
	typedef CCont::const_iterator CCIter;

	static bool IsMd( const Container&d ) 
	    { return( d.Type()==Container::MD ); };
	static bool IsLoop( const Container&d ) 
	    { return( d.Type()==Container::LOOP ); };
    public:
	struct SkipDeleted { bool operator()(const Container&d) const {return( !d.Delete());}};
	static SkipDeleted SkipDel;
	static bool NotDeleted( const Container&d ) { return( !d.Delete() ); };
	static bool IsRealDisk( const Container&d ) 
	    { return( d.Type()==Container::DISK ); };

	Storage( bool ronly=false, bool autodetect=true );
	virtual ~Storage();

// iterators over container 
    protected:
	// protected typedefs for iterators over container
	template< class Pred > 
	    struct ConstContainerPI { typedef ContainerIter<Pred, CCIter> type; };
	typedef CheckFnc<const Container> CheckFncCont;
	typedef CheckerIterator< CheckFncCont, ConstContainerPI<CheckFncCont>::type, 
	                         CCIter, Container > ConstContPIterator;
	template< class Pred > 
	    struct ContainerPI { typedef ContainerIter<Pred, CIter> type; };
	template< class Pred > 
	    struct ContainerI 
		{ typedef ContainerDerIter<Pred, typename ContainerPI<Pred>::type, 
					   Container> type; };
	typedef CheckerIterator< CheckFncCont, ContainerPI<CheckFncCont>::type, 
	                         CIter, Container > ContPIterator;
	typedef DerefIterator<ContPIterator,Container> ContIterator;
	typedef IterPair<ContIterator> CIPair;

    public:
	// public typedefs for iterators over containers
	template< class Pred > 
	    struct ConstContainerI 
		{ typedef ContainerDerIter<Pred, typename ConstContainerPI<Pred>::type, 
					   const Container> type; };
	template< class Pred >
	    struct ContCondIPair { typedef MakeCondIterPair<Pred, 
	                           typename ConstContainerI<Pred>::type> type;};
	typedef DerefIterator<ConstContPIterator,const Container> ConstContIterator;
	typedef IterPair<ConstContIterator> ConstContPair;

	// public member functions for iterators over containers 
	ConstContPair ContPair( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContPair( ContBegin( CheckFnc ), ContEnd( CheckFnc ) ));
	    }
	ConstContIterator ContBegin( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContPIterator( cont.begin(), cont.end(), CheckFnc )) );
	    }
	ConstContIterator ContEnd( bool (* CheckFnc)( const Container& )=NULL ) const
	    { 
	    return( ConstContIterator( ConstContPIterator( cont.begin(), cont.end(), CheckFnc, true )) );
	    }
	template< class Pred > typename ContCondIPair<Pred>::type ContCondPair( const Pred& p ) const
	    {
	    return( ContCondIPair<Pred>::type( ContCondBegin( p ), ContCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstContainerI<Pred>::type ContCondBegin( const Pred& p ) const
	    {
	    return( ConstContainerI<Pred>::type( ConstContainerPI<Pred>::type( cont.begin(), cont.end(), p )) );
	    }
	template< class Pred > typename ConstContainerI<Pred>::type ContCondEnd( const Pred& p ) const
	    {
	    return( ConstContainerI<Pred>::type( ConstContainerPI<Pred>::type( cont.begin(), cont.end(), p, true )) );
	    }
    protected:
	// protected member functions for iterators over containers 
	CIPair CPair( bool (* CheckFnc)( const Container& )=NULL )
	    {
	    return( CIPair( CBegin(CheckFnc), CEnd(CheckFnc) ));
	    }
	ContIterator CBegin( bool (* CheckFnc)( const Container& )=NULL )
	    { 
	    return( ContIterator( ContPIterator( cont.begin(), cont.end(), CheckFnc )) );
	    }
	ContIterator CEnd( bool (* CheckFnc)( const Container& )=NULL )
	    { 
	    return( ContIterator( ContPIterator( cont.begin(), cont.end(), CheckFnc, true )) );
	    }

// iterators over disks 
    protected:
	// protected typedefs for iterators over disks
	typedef CastCheckIterator<CCIter, Container::DISK, const Disk *> ContainerCDiskIter;
	template< class Pred > 
	    struct ConstDiskPI { typedef ContainerIter<Pred, ContainerCDiskIter> type; };
	typedef CastCheckIterator<CIter, Container::DISK, Disk *> ContainerDiskIter;
	template< class Pred > 
	    struct DiskPI { typedef ContainerIter<Pred, ContainerDiskIter> type; };
	template< class Pred > 
	    struct DiskI { typedef ContainerDerIter<Pred, typename DiskPI<Pred>::type, Disk> type; };
	typedef CheckFnc<const Disk> CheckFncDisk;
	typedef CheckerIterator< CheckFncDisk, ConstDiskPI<CheckFncDisk>::type, 
	                         ContainerCDiskIter, Disk > ConstDiskPIterator;

    public:
	// public typedefs for iterators over disks
	typedef DerefIterator<ConstDiskPIterator,const Disk> ConstDiskIterator;
	template< class Pred > 
	    struct ConstDiskI 
		{ typedef ContainerDerIter<Pred, typename ConstDiskPI<Pred>::type, 
					   const Disk> type; };
	template< class Pred >
	    struct DiskCondIPair { typedef MakeCondIterPair<Pred, typename ConstDiskI<Pred>::type> type; };
	typedef IterPair<ConstDiskIterator> ConstDiskPair;

	// public member functions for iterators over disks
	ConstDiskPair DiskPair( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    return( ConstDiskPair( DiskBegin( CheckFnc ), DiskEnd( CheckFnc ) ));
	    }
	ConstDiskIterator DiskBegin( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( cont.begin(), cont.end() ),
	                                    ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( ConstDiskIterator( ConstDiskPIterator( p, CheckFnc )) );
	    }
	ConstDiskIterator DiskEnd( bool (* CheckFnc)( const Disk& )=NULL ) const
	    { 
	    IterPair<ContainerCDiskIter> p( ContainerCDiskIter( cont.begin(), cont.end() ),
	                                    ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( ConstDiskIterator( ConstDiskPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename DiskCondIPair<Pred>::type DiskCondPair( const Pred& p ) const
	    {
	    return( DiskCondIPair<Pred>::type( DiskCondBegin( p ), DiskCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstDiskI<Pred>::type DiskCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( cont.begin(), cont.end() ),
					       ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( ConstDiskI<Pred>::type( ConstDiskPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstDiskI<Pred>::type DiskCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( cont.begin(), cont.end() ),
					       ContainerCDiskIter( cont.begin(), cont.end(), true ));
	    return( ConstDiskI<Pred>::type( ConstDiskPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over disks

// iterators over LVM VGs 
    protected:
	// protected typedefs for iterators over LVM VGs
	typedef CastCheckIterator<CCIter, Container::LVM, const LvmVg *> ContainerCLvmVgIter;
	template< class Pred > 
	    struct ConstLvmVgPI { typedef ContainerIter<Pred, ContainerCLvmVgIter> type; };
	typedef CastCheckIterator<CIter, Container::DISK, LvmVg *> ContainerLvmVgIter;
	template< class Pred > 
	    struct LvmVgPI { typedef ContainerIter<Pred, ContainerLvmVgIter> type; };
	template< class Pred > 
	    struct LvmVgI { typedef ContainerDerIter<Pred, typename LvmVgPI<Pred>::type, LvmVg> type; };
	typedef CheckFnc<const LvmVg> CheckFncLvmVg;
	typedef CheckerIterator< CheckFncLvmVg, ConstLvmVgPI<CheckFncLvmVg>::type, 
	                         ContainerCLvmVgIter, LvmVg > ConstLvmVgPIterator;

    public:
	// public typedefs for iterators over LVM VGs
	typedef DerefIterator<ConstLvmVgPIterator,const LvmVg> ConstLvmVgIterator;
	template< class Pred > 
	    struct ConstLvmVgI 
		{ typedef ContainerDerIter<Pred, typename ConstLvmVgPI<Pred>::type, 
					   const LvmVg> type; };
	template< class Pred >
	    struct LvmVgCondIPair { typedef MakeCondIterPair<Pred, typename ConstLvmVgI<Pred>::type> type; };
	typedef IterPair<ConstLvmVgIterator> ConstLvmVgPair;

	// public member functions for iterators over LVM VGs
	ConstLvmVgPair LvmVgPair( bool (* CheckFnc)( const LvmVg& )=NULL ) const
	    { 
	    return( ConstLvmVgPair( LvmVgBegin( CheckFnc ), LvmVgEnd( CheckFnc ) ));
	    }
	ConstLvmVgIterator LvmVgBegin( bool (* CheckFnc)( const LvmVg& )=NULL ) const
	    { 
	    IterPair<ContainerCLvmVgIter> p( ContainerCLvmVgIter( cont.begin(), cont.end() ),
	                                     ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( ConstLvmVgIterator( ConstLvmVgPIterator( p, CheckFnc )) );
	    }
	ConstLvmVgIterator LvmVgEnd( bool (* CheckFnc)( const LvmVg& )=NULL ) const
	    { 
	    IterPair<ContainerCLvmVgIter> p( ContainerCLvmVgIter( cont.begin(), cont.end() ),
	                                     ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( ConstLvmVgIterator( ConstLvmVgPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename LvmVgCondIPair<Pred>::type LvmVgCondPair( const Pred& p ) const
	    {
	    return( LvmVgCondIPair<Pred>::type( LvmVgCondBegin( p ), LvmVgCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstLvmVgI<Pred>::type LvmVgCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCLvmVgIter> pair( ContainerCLvmVgIter( cont.begin(), cont.end() ),
					        ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( ConstLvmVgI<Pred>::type( ConstLvmVgPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstLvmVgI<Pred>::type LvmVgCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCLvmVgIter> pair( ContainerCLvmVgIter( cont.begin(), cont.end() ),
					        ContainerCLvmVgIter( cont.begin(), cont.end(), true ));
	    return( ConstLvmVgI<Pred>::type( ConstLvmVgPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over LVM VGs

// iterators over EVMS container 
    protected:
	// protected typedefs for iterators over EVMS container
	typedef CastCheckIterator<CCIter, Container::EVMS, const Evms *> ContainerCEvmsIter;
	template< class Pred > 
	    struct ConstEvmsPI { typedef ContainerIter<Pred, ContainerCEvmsIter> type; };
	typedef CastCheckIterator<CIter, Container::DISK, Evms *> ContainerEvmsIter;
	template< class Pred > 
	    struct EvmsPI { typedef ContainerIter<Pred, ContainerEvmsIter> type; };
	template< class Pred > 
	    struct EvmsI { typedef ContainerDerIter<Pred, typename EvmsPI<Pred>::type, Evms> type; };
	typedef CheckFnc<const Evms> CheckFncEvms;
	typedef CheckerIterator< CheckFncEvms, ConstEvmsPI<CheckFncEvms>::type, 
	                         ContainerCEvmsIter, Evms > ConstEvmsPIterator;

    public:
	// public typedefs for iterators over EVMS container
	typedef DerefIterator<ConstEvmsPIterator,const Evms> ConstEvmsIterator;
	template< class Pred > 
	    struct ConstEvmsI 
		{ typedef ContainerDerIter<Pred, typename ConstEvmsPI<Pred>::type, 
					   const Evms> type; };
	template< class Pred >
	    struct EvmsCondIPair { typedef MakeCondIterPair<Pred, typename ConstEvmsI<Pred>::type> type; };
	typedef IterPair<ConstEvmsIterator> ConstEvmsPair;

	// public member functions for iterators over EVMS container
	ConstEvmsPair EvmsPair( bool (* CheckFnc)( const Evms& )=NULL ) const
	    { 
	    return( ConstEvmsPair( EvmsBegin( CheckFnc ), EvmsEnd( CheckFnc ) ));
	    }
	ConstEvmsIterator EvmsBegin( bool (* CheckFnc)( const Evms& )=NULL ) const
	    { 
	    IterPair<ContainerCEvmsIter> p( ContainerCEvmsIter( cont.begin(), cont.end() ),
	                                    ContainerCEvmsIter( cont.begin(), cont.end(), true ));
	    return( ConstEvmsIterator( ConstEvmsPIterator( p, CheckFnc )) );
	    }
	ConstEvmsIterator EvmsEnd( bool (* CheckFnc)( const Evms& )=NULL ) const
	    { 
	    IterPair<ContainerCEvmsIter> p( ContainerCEvmsIter( cont.begin(), cont.end() ),
	                                    ContainerCEvmsIter( cont.begin(), cont.end(), true ));
	    return( ConstEvmsIterator( ConstEvmsPIterator( p, CheckFnc, true )) );
	    }
	template< class Pred > typename EvmsCondIPair<Pred>::type EvmsCondPair( const Pred& p ) const
	    {
	    return( EvmsCondIPair<Pred>::type( EvmsCondBegin( p ), EvmsCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstEvmsI<Pred>::type EvmsCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCEvmsIter> pair( ContainerCEvmsIter( cont.begin(), cont.end() ),
					       ContainerCEvmsIter( cont.begin(), cont.end(), true ));
	    return( ConstEvmsI<Pred>::type( ConstEvmsPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstEvmsI<Pred>::type EvmsCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCEvmsIter> pair( ContainerCEvmsIter( cont.begin(), cont.end() ),
					       ContainerCEvmsIter( cont.begin(), cont.end(), true ));
	    return( ConstEvmsI<Pred>::type( ConstEvmsPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over EVMS container

// iterators over volumes 
    protected:
	// protected typedefs for iterators over volumes
	typedef ListListIterator<Container::ConstPlainIterator, ConstContIterator> ConstVolInter;
	template< class Pred > 
	    struct ConstVolumePI { typedef ContainerIter<Pred, ConstVolInter> type; };
	typedef CheckFnc<const Volume> CheckFncVol;
	typedef CheckerIterator< CheckFncVol, ConstVolumePI<CheckFncVol>::type, 
	                         ConstVolInter, Volume > ConstVolPIterator;
	typedef ListListIterator<Container::PlainIterator, ContIterator> VolPart;
	template< class Pred > 
	    struct VolumeI { typedef ContainerIter<Pred, VolPart> type; };
	typedef CheckerIterator< CheckFncVol, VolumeI<CheckFncVol>::type, 
	                         VolPart, Volume > VolIterator;
	typedef IterPair<VolIterator> VIPair;

    public:
	// public typedefs for iterators over volumes
	template< class Pred > 
	    struct ConstVolumeI { typedef ContainerDerIter<Pred, typename ConstVolumePI<Pred>::type, const Volume> type; };
	template< class Pred >
	    struct VolCondIPair { typedef MakeCondIterPair<Pred, typename ConstVolumeI<Pred>::type> type;};
	typedef DerefIterator<ConstVolPIterator,const Volume> ConstVolIterator;
	typedef IterPair<ConstVolIterator> ConstVolPair;

	// public member functions for iterators over volumes
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
	    IterPair<ConstVolInter> p( (ConstVolInter( ContPair( CheckCnt ))),
				       (ConstVolInter( ContPair( CheckCnt ), true )));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol )));
	    }
	ConstVolIterator VolEnd( bool (* CheckCnt)( const Container& )) const
	    { 
	    return( VolEnd( NULL, CheckCnt ) );
	    }
	ConstVolIterator VolEnd( bool (* CheckVol)( const Volume& )=NULL, 
	                         bool (* CheckCnt)( const Container& )=NULL) const
	    { 
	    IterPair<ConstVolInter> p( (ConstVolInter( ContPair( CheckCnt ))),
				       (ConstVolInter( ContPair( CheckCnt ), true )));
	    return( ConstVolIterator( ConstVolPIterator(p, CheckVol, true )));
	    }
	template< class Pred > typename VolCondIPair<Pred>::type VolCondPair( const Pred& p ) const
	    {
	    return( VolCondIPair<Pred>::type( VolCondBegin( p ), VolCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type VolCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstVolInter> pair( (ConstVolInter( ContPair())),
					  (ConstVolInter( ContPair(), true )));
	    return( ConstVolumeI<Pred>::type( ConstVolumePI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstVolumeI<Pred>::type VolCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstVolInter> pair( (ConstVolInter( ContPair())),
					  (ConstVolInter( ContPair(), true )));
	    return( ConstVolumeI<Pred>::type( ConstVolumePI<Pred>::type(pair, p, true )) );
	    }

    protected:
	// protected member functions for iterators over volumes
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
	    IterPair<VolPart> p( (VolPart( CPair( CheckCnt ))),
				 (VolPart( CPair( CheckCnt ), true )));
	    return( VolIterator( p, CheckVol ));
	    }
	VolIterator VEnd( bool (* CheckCnt)( const Container& ))
	    { 
	    return( VEnd( NULL, CheckCnt ) );
	    }
	VolIterator VEnd( bool (* CheckVol)( const Volume& )=NULL, 
			  bool (* CheckCnt)( const Container& )=NULL)
	    { 
	    IterPair<VolPart> p( (VolPart( CPair( CheckCnt ))),
				 (VolPart( CPair( CheckCnt ), true )));
	    return( VolIterator( p, CheckVol, true ));
	    }

// iterators over partitions
    protected:
	// protected typedefs for iterators over partitions
	typedef ListListIterator<Container::ConstPlainIterator, ConstDiskIterator> ConstPartInter;
	typedef CastIterator<ConstPartInter, Partition *> ConstPartInter2;
	template< class Pred > 
	    struct ConstPartitionPI { typedef ContainerIter<Pred, ConstPartInter2> type; };
	typedef CheckFnc<const Partition> CheckFncPartition;
	typedef CheckerIterator< CheckFncPartition, ConstPartitionPI<CheckFncPartition>::type, 
	                         ConstPartInter2, Partition > ConstPartPIterator;
    public:
	// public typedefs for iterators over partitions
	template< class Pred > 
	    struct ConstPartitionI 
		{ typedef ContainerDerIter<Pred, typename ConstPartitionPI<Pred>::type, 
		                           const Partition> type; };
	template< class Pred >
	    struct PartCondIPair 
		{ typedef MakeCondIterPair<Pred, typename ConstPartitionI<Pred>::type> type;};
	typedef DerefIterator<ConstPartPIterator, const Partition> ConstPartIterator;
	typedef IterPair<ConstPartIterator> ConstPartPair;

	// public member functions for iterators over partitions
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
	    IterPair<ConstPartInter2> p( (ConstPartInter(DiskPair( CheckDisk ))),
					 (ConstPartInter(DiskPair( CheckDisk ), true )));
	    return( ConstPartIterator( ConstPartPIterator(p, CheckPart )));
	    }
	ConstPartIterator PartEnd( bool (* CheckDisk)( const Disk& )) const
	    { 
	    return( PartEnd( NULL, CheckDisk ) );
	    }
	ConstPartIterator PartEnd( bool (* CheckPart)( const Partition& )=NULL, 
	                           bool (* CheckDisk)( const Disk& )=NULL) const
	    { 
	    IterPair<ConstPartInter2> p( (ConstPartInter(DiskPair( CheckDisk ))),
					 (ConstPartInter(DiskPair( CheckDisk ), true )));
	    return( ConstPartIterator( ConstPartPIterator(p, CheckPart, true )));
	    }
	template< class Pred > typename PartCondIPair<Pred>::type PartCondPair( const Pred& p ) const
	    {
	    return( PartCondIPair<Pred>::type( PartCondBegin( p ), PartCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstPartitionI<Pred>::type PartCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstPartInter2> pair( (ConstPartInter( DiskPair())),
					    (ConstPartInter( DiskPair(), true )));
	    return( ConstPartitionI<Pred>::type( ConstPartitionPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstPartitionI<Pred>::type PartCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstPartInter2> pair( (ConstPartInter( DiskPair())),
					    (ConstPartInter( DiskPair(), true )));
	    return( ConstPartitionI<Pred>::type( ConstPartitionPI<Pred>::type(pair, p, true )) );
	    }

// iterators over LVM LVs
    protected:
	// protected typedefs for iterators over LVM LVs
	typedef ListListIterator<Container::ConstPlainIterator, ConstLvmVgIterator> ConstLvmLvInter;
	typedef CastIterator<ConstLvmLvInter, LvmLv *> ConstLvmLvInter2;
	template< class Pred > 
	    struct ConstLvmLvPI { typedef ContainerIter<Pred, ConstLvmLvInter2> type; };
	typedef CheckFnc<const LvmLv> CheckFncLvmLv;
	typedef CheckerIterator< CheckFncLvmLv, ConstLvmLvPI<CheckFncLvmLv>::type, 
	                         ConstLvmLvInter2, LvmLv > ConstLvmLvPIterator;
    public:
	// public typedefs for iterators over LVM LVs
	template< class Pred > 
	    struct ConstLvmLvI 
		{ typedef ContainerDerIter<Pred, typename ConstLvmLvPI<Pred>::type, 
		                           const LvmLv> type; };
	template< class Pred >
	    struct LvmLvCondIPair 
		{ typedef MakeCondIterPair<Pred, typename ConstLvmLvI<Pred>::type> type;};
	typedef DerefIterator<ConstLvmLvPIterator, const LvmLv> ConstLvmLvIterator;
	typedef IterPair<ConstLvmLvIterator> ConstLvmLvPair;

	// public member functions for iterators over LVM LVs
	ConstLvmLvPair LvmLvPair( bool (* CheckLvmVg)( const LvmVg& )) const
	    { 
	    return( ConstLvmLvPair( LvmLvBegin( CheckLvmVg ), LvmLvEnd( CheckLvmVg ) ));
	    }
	ConstLvmLvPair LvmLvPair( bool (* CheckLvmLv)( const LvmLv& )=NULL,
				  bool (* CheckLvmVg)( const LvmVg& )=NULL) const
	    { 
	    return( ConstLvmLvPair( LvmLvBegin( CheckLvmLv, CheckLvmVg ),
	                            LvmLvEnd( CheckLvmLv, CheckLvmVg ) ));
	    }
	ConstLvmLvIterator LvmLvBegin( bool (* CheckLvmVg)( const LvmVg& )) const
	    { 
	    return( LvmLvBegin( NULL, CheckLvmVg ) );
	    }
	ConstLvmLvIterator LvmLvBegin( bool (* CheckLvmLv)( const LvmLv& )=NULL, 
	                               bool (* CheckLvmVg)( const LvmVg& )=NULL) const
	    {
	    IterPair<ConstLvmLvInter2> p( (ConstLvmLvInter(LvmVgPair( CheckLvmVg ))),
					  (ConstLvmLvInter(LvmVgPair( CheckLvmVg ), true )));
	    return( ConstLvmLvIterator( ConstLvmLvPIterator(p, CheckLvmLv )));
	    }
	ConstLvmLvIterator LvmLvEnd( bool (* CheckLvmVg)( const LvmVg& )) const
	    { 
	    return( LvmLvEnd( NULL, CheckLvmVg ) );
	    }
	ConstLvmLvIterator LvmLvEnd( bool (* CheckLvmLv)( const LvmLv& )=NULL, 
	                             bool (* CheckLvmVg)( const LvmVg& )=NULL) const
	    { 
	    IterPair<ConstLvmLvInter2> p( (ConstLvmLvInter(LvmVgPair( CheckLvmVg ))),
					  (ConstLvmLvInter(LvmVgPair( CheckLvmVg ), true )));
	    return( ConstLvmLvIterator( ConstLvmLvPIterator(p, CheckLvmLv, true )));
	    }
	template< class Pred > typename LvmLvCondIPair<Pred>::type LvmLvCondPair( const Pred& p ) const
	    {
	    return( LvmLvCondIPair<Pred>::type( LvmLvCondBegin( p ), LvmLvCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstLvmLvI<Pred>::type LvmLvCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstLvmLvInter2> pair( (ConstLvmLvInter( LvmVgPair())),
					     (ConstLvmLvInter( LvmVgPair(), true )));
	    return( ConstLvmLvI<Pred>::type( ConstLvmLvPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstLvmLvI<Pred>::type LvmLvCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstLvmLvInter2> pair( (ConstLvmLvInter( LvmVgPair())),
					     (ConstLvmLvInter( LvmVgPair(), true )));
	    return( ConstLvmLvI<Pred>::type( ConstLvmLvPI<Pred>::type(pair, p, true )) );
	    }

// iterators over EVMS volumes
    protected:
	// protected typedefs for iterators over EVMS volumes
	typedef ListListIterator<Container::ConstPlainIterator, ConstEvmsIterator> ConstEvmsVolInter;
	typedef CastIterator<ConstEvmsVolInter, EvmsVol *> ConstEvmsVolInter2;
	template< class Pred > 
	    struct ConstEvmsVolPI { typedef ContainerIter<Pred, ConstEvmsVolInter2> type; };
	typedef CheckFnc<const EvmsVol> CheckFncEvmsVol;
	typedef CheckerIterator< CheckFncEvmsVol, ConstEvmsVolPI<CheckFncEvmsVol>::type, 
	                         ConstEvmsVolInter2, EvmsVol > ConstEvmsVolPIterator;
    public:
	// public typedefs for iterators over EVMS volumes
	template< class Pred > 
	    struct ConstEvmsVolI 
		{ typedef ContainerDerIter<Pred, typename ConstEvmsVolPI<Pred>::type, 
		                           const EvmsVol> type; };
	template< class Pred >
	    struct EvmsVolCondIPair 
		{ typedef MakeCondIterPair<Pred, typename ConstEvmsVolI<Pred>::type> type;};
	typedef DerefIterator<ConstEvmsVolPIterator, const EvmsVol> ConstEvmsVolIterator;
	typedef IterPair<ConstEvmsVolIterator> ConstEvmsVolPair;

	// public member functions for iterators over EVMS volumes
	ConstEvmsVolPair EvmsVolPair( bool (* CheckEvms)( const Evms& )) const
	    { 
	    return( ConstEvmsVolPair( EvmsVolBegin( CheckEvms ), EvmsVolEnd( CheckEvms ) ));
	    }
	ConstEvmsVolPair EvmsVolPair( bool (* CheckEvmsVol)( const EvmsVol& )=NULL,
				      bool (* CheckEvms)( const Evms& )=NULL) const
	    { 
	    return( ConstEvmsVolPair( EvmsVolBegin( CheckEvmsVol, CheckEvms ),
	                              EvmsVolEnd( CheckEvmsVol, CheckEvms ) ));
	    }
	ConstEvmsVolIterator EvmsVolBegin( bool (* CheckEvms)( const Evms& )) const
	    { 
	    return( EvmsVolBegin( NULL, CheckEvms ) );
	    }
	ConstEvmsVolIterator EvmsVolBegin( bool (* CheckEvmsVol)( const EvmsVol& )=NULL, 
	                                   bool (* CheckEvms)( const Evms& )=NULL) const
	    {
	    IterPair<ConstEvmsVolInter2> p( (ConstEvmsVolInter(EvmsPair( CheckEvms ))),
					    (ConstEvmsVolInter(EvmsPair( CheckEvms ), true )));
	    return( ConstEvmsVolIterator( ConstEvmsVolPIterator(p, CheckEvmsVol )));
	    }
	ConstEvmsVolIterator EvmsVolEnd( bool (* CheckEvms)( const Evms& )) const
	    { 
	    return( EvmsVolEnd( NULL, CheckEvms ) );
	    }
	ConstEvmsVolIterator EvmsVolEnd( bool (* CheckEvmsVol)( const EvmsVol& )=NULL, 
	                                 bool (* CheckEvms)( const Evms& )=NULL) const
	    { 
	    IterPair<ConstEvmsVolInter2> p( (ConstEvmsVolInter(EvmsPair( CheckEvms ))),
					    (ConstEvmsVolInter(EvmsPair( CheckEvms ), true )));
	    return( ConstEvmsVolIterator( ConstEvmsVolPIterator(p, CheckEvmsVol, true )));
	    }
	template< class Pred > typename EvmsVolCondIPair<Pred>::type EvmsVolCondPair( const Pred& p ) const
	    {
	    return( EvmsVolCondIPair<Pred>::type( EvmsVolCondBegin( p ), EvmsVolCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstEvmsVolI<Pred>::type EvmsVolCondBegin( const Pred& p ) const
	    {
	    IterPair<ConstEvmsVolInter2> pair( (ConstEvmsVolInter( EvmsPair())),
					       (ConstEvmsVolInter( EvmsPair(), true )));
	    return( ConstEvmsVolI<Pred>::type( ConstEvmsVolPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstEvmsVolI<Pred>::type EvmsVolCondEnd( const Pred& p ) const
	    {
	    IterPair<ConstEvmsVolInter2> pair( (ConstEvmsVolInter( EvmsPair())),
					       (ConstEvmsVolInter( EvmsPair(), true )));
	    return( ConstEvmsVolI<Pred>::type( ConstEvmsVolPI<Pred>::type(pair, p, true )) );
	    }

// iterators over software raid devices
    protected:
	// protected typedefs for iterators over software raid devices
	typedef CastIterator<ConstVolInter, Md *> ConstMdInter;
	template< class Pred > 
	    struct ConstMdPI { typedef ContainerIter<Pred, 
	                                             ConstMdInter> type; };
	typedef CheckFnc<const Md> CheckFncMd;
	typedef CheckerIterator< CheckFncMd, ConstMdPI<CheckFncMd>::type,
	                         ConstMdInter, Md > ConstMdPIterator;
    public:
	// public typedefs for iterators over software raid devices
	template< class Pred > 
	    struct ConstMdI 
		{ typedef ContainerDerIter<Pred, typename ConstMdPI<Pred>::type,
		                           const Md> type; };
	template< class Pred >
	    struct MdCondIPair 
		{ typedef MakeCondIterPair<Pred, typename ConstMdI<Pred>::type> type;};
	typedef DerefIterator<ConstMdPIterator, const Md> ConstMdIterator;
	typedef IterPair<ConstMdIterator> ConstMdPair;

	// public member functions for iterators over software raid devices
	ConstMdPair MdPair( bool (* CheckMd)( const Md& )=NULL ) const
	    { 
	    return( ConstMdPair( MdBegin( CheckMd ), MdEnd( CheckMd ) ));
	    }
	ConstMdIterator MdBegin( bool (* CheckMd)( const Md& )=NULL ) const
	    {
	    ConstVolInter b( ContPair( IsMd ) );
	    ConstVolInter e( ContPair( IsMd ), true );
	    IterPair<ConstMdInter> p( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( ConstMdIterator( ConstMdPIterator(p, CheckMd )));
	    }
	ConstMdIterator MdEnd( bool (* CheckMd)( const Md& )=NULL ) const
	    { 
	    ConstVolInter b( ContPair( IsMd ) );
	    ConstVolInter e( ContPair( IsMd ), true );
	    IterPair<ConstMdInter> p( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( ConstMdIterator( ConstMdPIterator(p, CheckMd, true )));
	    }
	template< class Pred > typename MdCondIPair<Pred>::type MdCondPair( const Pred& p ) const
	    {
	    return( MdCondIPair<Pred>::type( MdCondBegin( p ), MdCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstMdI<Pred>::type MdCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( ContPair( IsMd ) );
	    ConstVolInter e( ContPair( IsMd ), true );
	    IterPair<ConstMdInter> pair( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( ConstMdI<Pred>::type( ConstMdPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstMdI<Pred>::type MdCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( ContPair( IsMd ) );
	    ConstVolInter e( ContPair( IsMd ), true );
	    IterPair<ConstMdInter> pair( (ConstMdInter(b)), (ConstMdInter(e)) );
	    return( ConstMdI<Pred>::type( ConstMdPI<Pred>::type(pair, p, true )) );
	    }

// iterators over file based loop devices
    protected:
	// protected typedefs for iterators over file based loop devices
	typedef CastIterator<ConstVolInter, Loop *> ConstLoopInter;
	template< class Pred > 
	    struct ConstLoopPI { typedef ContainerIter<Pred, 
	                                             ConstLoopInter> type; };
	typedef CheckFnc<const Loop> CheckFncLoop;
	typedef CheckerIterator< CheckFncLoop, ConstLoopPI<CheckFncLoop>::type,
	                         ConstLoopInter, Loop > ConstLoopPIterator;
    public:
	// public typedefs for iterators over file based loop devices
	template< class Pred > 
	    struct ConstLoopI 
		{ typedef ContainerDerIter<Pred, typename ConstLoopPI<Pred>::type,
		                           const Loop> type; };
	template< class Pred >
	    struct LoopCondIPair 
		{ typedef MakeCondIterPair<Pred, typename ConstLoopI<Pred>::type> type;};
	typedef DerefIterator<ConstLoopPIterator, const Loop> ConstLoopIterator;
	typedef IterPair<ConstLoopIterator> ConstLoopPair;

	// public member functions for iterators over file based loop devices
	ConstLoopPair LoopPair( bool (* CheckLoop)( const Loop& )=NULL ) const
	    { 
	    return( ConstLoopPair( LoopBegin( CheckLoop ), LoopEnd( CheckLoop ) ));
	    }
	ConstLoopIterator LoopBegin( bool (* CheckLoop)( const Loop& )=NULL ) const
	    {
	    ConstVolInter b( ContPair( IsLoop ) );
	    ConstVolInter e( ContPair( IsLoop ), true );
	    IterPair<ConstLoopInter> p( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( ConstLoopIterator( ConstLoopPIterator(p, CheckLoop )));
	    }
	ConstLoopIterator LoopEnd( bool (* CheckLoop)( const Loop& )=NULL ) const
	    { 
	    ConstVolInter b( ContPair( IsLoop ) );
	    ConstVolInter e( ContPair( IsLoop ), true );
	    IterPair<ConstLoopInter> p( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( ConstLoopIterator( ConstLoopPIterator(p, CheckLoop, true )));
	    }
	template< class Pred > typename LoopCondIPair<Pred>::type LoopCondPair( const Pred& p ) const
	    {
	    return( LoopCondIPair<Pred>::type( LoopCondBegin( p ), LoopCondEnd( p ) ) );
	    }
	template< class Pred > typename ConstLoopI<Pred>::type LoopCondBegin( const Pred& p ) const
	    {
	    ConstVolInter b( ContPair( IsLoop ) );
	    ConstVolInter e( ContPair( IsLoop ), true );
	    IterPair<ConstLoopInter> pair( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( ConstLoopI<Pred>::type( ConstLoopPI<Pred>::type(pair, p) ) );
	    }
	template< class Pred > typename ConstLoopI<Pred>::type LoopCondEnd( const Pred& p ) const
	    {
	    ConstVolInter b( ContPair( IsLoop ) );
	    ConstVolInter e( ContPair( IsLoop ), true );
	    IterPair<ConstLoopInter> pair( (ConstLoopInter(b)), (ConstLoopInter(e)) );
	    return( ConstLoopI<Pred>::type( ConstLoopPI<Pred>::type(pair, p, true )) );
	    }

    protected:
	// protected internal member functions
	void AutodetectDisks();
	int AddDisk( const string& Name );

	// protected internal member variables
	bool readonly;
	CCont cont;
    };

Storage::SkipDeleted Storage::SkipDel;


#endif
