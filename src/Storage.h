#ifndef STORAGE_H
#define STORAGE_H

#include <iostream>
#include <list>

#include "y2storage/Container.h"
#include "y2storage/Disk.h"
#include "y2storage/Md.h"
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
	template< class Pred > typename ConstContainerI<Pred>::type ContCondBegin( const Pred& p ) const
	    {
	    return( ConstContainerI<Pred>::type( ConstContainerPI<Pred>::type( Disks.begin(), Disks.end(), p )) );
	    }
	template< class Pred > typename ConstContainerI<Pred>::type ContCondEnd( const Pred& p ) const
	    {
	    return( ConstContainerI<Pred>::type( ConstContainerPI<Pred>::type( Disks.begin(), Disks.end(), p, true )) );
	    }
    protected:
	// protected member functions for iterators over containers 
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
	template< class Pred > typename ConstDiskI<Pred>::type DiskCondBegin( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( Disks.begin(), Disks.end() ),
					       ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( ConstDiskI<Pred>::type( ConstDiskPI<Pred>::type( pair, p )) );
	    }
	template< class Pred > typename ConstDiskI<Pred>::type DiskCondEnd( const Pred& p ) const
	    {
	    IterPair<ContainerCDiskIter> pair( ContainerCDiskIter( Disks.begin(), Disks.end() ),
					       ContainerCDiskIter( Disks.begin(), Disks.end(), true ));
	    return( ConstDiskI<Pred>::type( ConstDiskPI<Pred>::type( pair, p, true )) );
	    }
    protected:
	// protected member functions for iterators over disks

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

    protected:
	// protected internal member functions
	void AutodetectDisks();
	int AddDisk( const string& Name );

	// protected internal member variables
	bool readonly;
	CCont Disks;
    };

Storage::SkipDeleted Storage::SkipDel;


#endif
