
#include <iostream>

#include <y2storage/Storage.h>
#include <y2storage/ListListIterator.h>

using namespace std;
using namespace storage;

struct test_hdb { bool operator()(const Container&d) const {return( d.name().find( "hdb" )!=string::npos);}};
struct NotDeleted { bool operator()(const Container& d) const { return !d.deleted(); } };
struct Smaller5 { bool operator()(const Volume&d) const {return(d.nr()<5);}};
struct Smaller150 { bool operator()(const Disk&d) const {return(d.cylinders()<150);}};
struct Larger150 { bool operator()(const Disk&d) const {return(d.cylinders()>150);}};
struct Equal150 { bool operator()(const Disk&d) const {return(d.cylinders()==150);}};
struct Larger10 { bool operator()(const Partition&d) const {return(d.cylSize()>10);}};
struct DiskStart { bool operator()(const Partition&d) const {return(d.cylStart()==1);}};

template <class C> struct First 
    { bool operator()(const C&d) const {return(d.nr()==0);}};

struct Raid5 { bool operator()(const Md&d) const {return(d.personality()==storage::RAID5);}};
struct FileOther { bool operator()(const Loop&d) const 
    {return(d.loopFile().find( "other" )!=string::npos);}};
struct PVG1 { bool operator()(const LvmVg&d) const {return(d.numPv()>1);}};
struct LVG1 { bool operator()(const LvmVg&d) const {return(d.numLv()>1);}};
struct NoLv { bool operator()(const LvmVg&d) const {return(d.numLv()==0);}};

struct StripeG1 { bool operator()(const LvmLv&d) const {return(d.stripes()>1);}};
struct TestHaveA { bool operator()(const LvmLv&d) const 
    { return( d.name().find( "a" )!=string::npos); }};

template <class C> bool TestIsA( const C& d ) 
    { return( d.name().find( "a" )!=string::npos); };
template <class C> bool TestIsB( const C& d ) 
    { return( d.name().find( "b" )!=string::npos); };
template <class C> bool TestFalse( const C& d ) { return( false ); };
template <class C> bool TestTrue( const C& d ) { return( true ); };
bool TestLvG1( const LvmVg& d ) { return( d.numLv()>1 ); };

template <class pair> 
void PrintPair( ostream& s, const pair& p, const string& txt )
    {
    s << txt;
    cout << "pair empty:" << p.empty() << " length:" << p.length() << endl;
    for( typename pair::itype i=p.begin(); i!=p.end(); ++i )
	{
	s << *i << endl;
	}
    }

int
main( int argc_iv, char** argv_ppcv )
{
    Storage::initDefaultLogger();
    Storage Sto(Environment(true));
    for( Storage::ConstContIterator i=Sto.contBegin(); i!=Sto.contEnd(); ++i )
	{
	cout << *i << endl;
	}
    {
    struct test_hdb t;
    Storage::ContCondIPair<test_hdb>::type p=Sto.contCondPair<test_hdb>(t); 
    cout << "test_hdb pair empty:" << p.empty() << " length:" << p.length() << endl;
    for( Storage::ConstContainerI<test_hdb>::type i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i << endl;
	}
    }
    {
	NotDeleted NotDel;
	Storage::ContCondIPair<NotDeleted>::type p = Sto.contCondPair<NotDeleted>(NotDel);
	cout << "NotDeleted pair empty:" << p.empty() << " length:" << p.length() << endl;
	for (Storage::ConstContainerI<NotDeleted>::type i = p.begin(); i != p.end(); ++i)
	{
	    cout << *i << endl;
	}
    }
    struct tmp { 
	static bool TestIsEven( const Volume& d ) 
	    { return( d.nr()%2==0 ); }
	static bool TestIsUnven( const Volume& d ) 
	    { return( d.nr()%2!=0 ); }};
    {
    Storage::ConstContPair p = Sto.contPair( TestIsB<Container> );
    cout << "only B pair empty:" << p.empty() << " length:" << p.length() << endl;
    for( Storage::ConstContIterator i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i << endl;
	}
    p = Sto.contPair( TestIsA<Container> );
    cout << "only A pair empty:" << p.empty() << " length:" << p.length() << endl;
    for( Storage::ConstContIterator i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i << endl;
	}
    p = Sto.contPair( TestTrue<Container> );
    cout << "all empty:" << p.empty() << " length:" << p.length() << endl;
    for( Storage::ConstContIterator i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i << endl;
	}
    p = Sto.contPair( TestFalse<Container> );
    cout << "none empty:" << p.empty() << " length:" << p.length() << endl;
    for( Storage::ConstContIterator i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i << endl;
	}
    p = Sto.contPair();
    cout << "all again empty:" << p.empty() << " length:" << p.length() << endl;
    for( Storage::ConstContIterator i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i << endl;
	}
    p = Sto.contPair( Storage::notDeleted );
    cout << "not deleted empty:" << p.empty() << " length:" << p.length() << endl;
    for( Storage::ConstContIterator i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i << endl;
	}
    if( !p.empty() )
	{
	cout << "Partitions first Disk" << endl;
	Container::ConstVolPair d = Sto.contBegin()->volPair();
	cout << "pair empty:" << d.empty() << " length:" << d.length() << endl;
	for( Container::ConstVolIterator i=d.begin(); i!=d.end(); ++i )
	    {
	    cout << *i << endl;
	    }
	cout << "Partitions first Disk even" << endl;
	d = Sto.contBegin()->volPair(tmp::TestIsEven);
	cout << "pair empty:" << d.empty() << " length:" << d.length() << endl;
	for( Container::ConstVolIterator i=d.begin(); i!=d.end(); ++i )
	    {
	    cout << *i << endl;
	    }
	if( p.length()>2 )
	    {
	    cout << "Partitions third disk even" << endl;
	    //cout << "type(Sto.contBegin())=" << typeid(Sto.contBegin()).name() << endl;
	    //cout << "type(++Sto.contBegin())=" << typeid(++Sto.contBegin()).name() << endl;
	    d = (++(++Sto.contBegin()))->volPair(tmp::TestIsEven);
	    cout << "pair empty:" << d.empty() << " length:" << d.length() << endl;
	    for( Container::ConstVolIterator i=d.begin(); i!=d.end(); ++i )
		{
		cout << *i << endl;
		}
	    }
	}
    }
    {
    Storage::ConstVolPair p = Sto.volPair();
    PrintPair<Storage::ConstVolPair>( cout, p, "All Volumes\n" );
    cout << "All and previous Partitions" << endl;
    for( Storage::ConstVolIterator i=p.begin(); i!=p.end(); ++i )
	{
	cout << *i;
	Storage::ConstVolIterator j = i;
	//cout << endl << "type(j)=" << typeid(j).name() << endl;
	//cout << "type(--j)=" << typeid(--j).name() << endl;
	cout << " P Name:" << (*--j).device() << endl;
	}
    cout << "Inverted order" << endl;
    for( Storage::ConstVolIterator i=p.end(); i!=p.begin(); )
	{
	--i;
	cout << *i << endl;
	}
    p = Sto.volPair( Storage::notDeleted );
    PrintPair<Storage::ConstVolPair>( cout, p, "All Volumes on undel disks\n" );
    p = Sto.volPair( tmp::TestIsEven );
    PrintPair<Storage::ConstVolPair>( cout, p, "All Volumes with even numbers\n" );
    p = Sto.volPair( tmp::TestIsEven, Storage::notDeleted );
    PrintPair<Storage::ConstVolPair>( cout, p, "All Volumes with even numbers on undel disks\n" );
    cout << "Inverted order" << endl;
    for( Storage::ConstVolIterator i=p.end(); i!=p.begin(); )
	{
	--i;
	cout << *i << endl;
	}
    }
    {
    struct Smaller5 sm5;
    Storage::VolCondIPair<Smaller5>::type p = Sto.volCondPair<Smaller5>( sm5 );
    PrintPair<Storage::VolCondIPair<Smaller5>::type>( cout, p, "Smaller5 " );
    }
    {
    Storage::ConstDiskPair p = Sto.diskPair();
    PrintPair<Storage::ConstDiskPair>( cout, p, "Disks " );
    struct tmp { 
	static bool TestLarger150( const Disk& d ) 
	    { return( d.cylinders()>150 ); };
	static bool TestSmaller150( const Disk& d ) 
	    { return( d.cylinders()<150 ); };
	static bool TestEqual150( const Disk& d ) 
	    { return( d.cylinders()==150 ); };
	};
    p = Sto.diskPair(tmp::TestLarger150);
    PrintPair<Storage::ConstDiskPair>( cout, p, "Disks >150 " );
    p = Sto.diskPair(tmp::TestSmaller150);
    PrintPair<Storage::ConstDiskPair>( cout, p, "Disks <150 " );
    p = Sto.diskPair(tmp::TestEqual150);
    PrintPair<Storage::ConstDiskPair>( cout, p, "Disks ==150 " );
    }
    {
    Storage::DiskCondIPair<Larger150>::type p = Sto.diskCondPair<Larger150>( Larger150() );
    PrintPair<Storage::DiskCondIPair<Larger150>::type>( cout, p, "Disks >150 " );
    }
    {
    Storage::DiskCondIPair<Smaller150>::type p = Sto.diskCondPair<Smaller150>( Smaller150() );
    PrintPair<Storage::DiskCondIPair<Smaller150>::type>( cout, p, "Disks <150 " );
    }
    {
    Storage::DiskCondIPair<Equal150>::type p = Sto.diskCondPair<Equal150>( Equal150() );
    PrintPair<Storage::DiskCondIPair<Equal150>::type>( cout, p, "Disks ==150 " );
    }
    {
    Storage::ConstPartPair p = Sto.partPair();
    PrintPair<Storage::ConstPartPair>( cout, p, "Part " );
    struct tmp { 
	static bool TestStart( const Partition& d ) 
	    { return( d.cylStart()==1 ); };
	static bool TestLarger10( const Partition& d ) 
	    { return( d.cylSize()>10 ); };
	};
    p = Sto.partPair(tmp::TestLarger10);
    PrintPair<Storage::ConstPartPair>( cout, p, "Partition >10 " );
    p = Sto.partPair(tmp::TestStart);
    PrintPair<Storage::ConstPartPair>( cout, p, "Partition start " );
    p = Sto.partPair( tmp::TestStart, TestIsA<Disk> );
    PrintPair<Storage::ConstPartPair>( cout, p, "Partition IsA start " );
    p = Sto.partPair( tmp::TestStart, TestFalse<Disk> );
    PrintPair<Storage::ConstPartPair>( cout, p, "Partition impossible " );
    }
    {
    Storage::PartCondIPair<Larger10>::type p = Sto.partCondPair<Larger10>( Larger10() );
    PrintPair<Storage::PartCondIPair<Larger10>::type>( cout, p, "Partition >10 " );
    }
    {
    Storage::PartCondIPair<DiskStart>::type p = Sto.partCondPair<DiskStart>( DiskStart() );
    PrintPair<Storage::PartCondIPair<DiskStart>::type>( cout, p, "Partition start " );
    }
    {
    Storage::ConstMdPair p = Sto.mdPair();
    PrintPair<Storage::ConstMdPair>( cout, p, "Md " );
    struct tmp { 
	static bool TestFirst( const Md& d ) 
	    { return( d.nr()==0 ); };
	static bool TestRaid5( const Md& d ) 
	    { return( d.personality()==storage::RAID5 ); };
	};
    p = Sto.mdPair(tmp::TestFirst);
    PrintPair<Storage::ConstMdPair>( cout, p, "Md first " );
    p = Sto.mdPair(tmp::TestRaid5);
    PrintPair<Storage::ConstMdPair>( cout, p, "Md raid5 " );
    p = Sto.mdPair( TestFalse<Md> );
    PrintPair<Storage::ConstMdPair>( cout, p, "Md impossible " );
    }
    {
    Storage::MdCondIPair<First<Md> >::type p = Sto.mdCondPair<First<Md> >( First<Md>() );
    PrintPair<Storage::MdCondIPair<First<Md> >::type>( cout, p, "Md First " );
    }
    {
    Storage::MdCondIPair<Raid5>::type p = Sto.mdCondPair<Raid5>( Raid5() );
    PrintPair<Storage::MdCondIPair<Raid5>::type>( cout, p, "Md raid5 " );
    }
    {
    Storage::ConstLoopPair p = Sto.loopPair();
    PrintPair<Storage::ConstLoopPair>( cout, p, "Loop " );
    struct tmp { 
	static bool TestFirst( const Loop& d ) 
	    { return( d.nr()==0 ); };
	static bool TestUsc( const Loop& d ) 
	    { return( d.loopFile().find("_")!=string::npos ); };
	};
    p = Sto.loopPair(tmp::TestFirst);
    PrintPair<Storage::ConstLoopPair>( cout, p, "Loop first " );
    p = Sto.loopPair(tmp::TestUsc);
    PrintPair<Storage::ConstLoopPair>( cout, p, "Loop underscore " );
    p = Sto.loopPair( TestFalse<Loop> );
    PrintPair<Storage::ConstLoopPair>( cout, p, "Loop impossible " );
    }
    {
    Storage::LoopCondIPair<First<Loop> >::type p = Sto.loopCondPair<First<Loop> >( First<Loop>() );
    PrintPair<Storage::LoopCondIPair<First<Loop> >::type>( cout, p, "Loop First " );
    }
    {
    Storage::LoopCondIPair<FileOther>::type p = Sto.loopCondPair<FileOther>( FileOther() );
    PrintPair<Storage::LoopCondIPair<FileOther>::type>( cout, p, "Loop FileOther " );
    }
    {
    Storage::ConstLvmVgPair p = Sto.lvmVgPair();
    PrintPair<Storage::ConstLvmVgPair>( cout, p, "LvmVg " );
    struct tmp { 
	static bool TestPvGt1( const LvmVg& d ) 
	    { return( d.numPv()>1 ); };
	static bool TestLvGt1( const LvmVg& d ) 
	    { return( d.numLv()>1 ); };
	static bool TestEmpty( const LvmVg& d ) 
	    { return( d.numLv()==0 ); };
	};
    p = Sto.lvmVgPair(tmp::TestPvGt1);
    PrintPair<Storage::ConstLvmVgPair>( cout, p, "LvmVg PV>1 " );
    p = Sto.lvmVgPair(tmp::TestLvGt1);
    PrintPair<Storage::ConstLvmVgPair>( cout, p, "LvmVg LV>1 " );
    p = Sto.lvmVgPair(tmp::TestEmpty);
    PrintPair<Storage::ConstLvmVgPair>( cout, p, "LvmVg No LV " );
    }
    {
    Storage::LvmVgCondIPair<PVG1>::type p = Sto.lvmVgCondPair<PVG1>( PVG1() );
    PrintPair<Storage::LvmVgCondIPair<PVG1>::type>( cout, p, "LvmVg PV>1 " );
    }
    {
    Storage::LvmVgCondIPair<LVG1>::type p = Sto.lvmVgCondPair<LVG1>( LVG1() );
    PrintPair<Storage::LvmVgCondIPair<LVG1>::type>( cout, p, "LvmVg LV>1 " );
    }
    {
    Storage::LvmVgCondIPair<NoLv>::type p = Sto.lvmVgCondPair<NoLv>( NoLv() );
    PrintPair<Storage::LvmVgCondIPair<NoLv>::type>( cout, p, "LvmVg LV==0 " );
    }
    {
    Storage::ConstLvmLvPair p = Sto.lvmLvPair();
    PrintPair<Storage::ConstLvmLvPair>( cout, p, "LvmLv " );
    struct tmp { 
	static bool TestNameA( const LvmLv& d ) 
	    { return( d.name().find("a")!=string::npos ); };
	static bool TestStripeG1( const LvmLv& d ) 
	    { return( d.stripes()>1 ); };
	};
    p = Sto.lvmLvPair(tmp::TestNameA);
    PrintPair<Storage::ConstLvmLvPair>( cout, p, "LvmLv 'a' " );
    p = Sto.lvmLvPair(tmp::TestStripeG1);
    PrintPair<Storage::ConstLvmLvPair>( cout, p, "LvmLv S>1 " );
    p = Sto.lvmLvPair( tmp::TestNameA, TestLvG1 );
    PrintPair<Storage::ConstLvmLvPair>( cout, p, "LvmLv 'a' LV>1 " );
    p = Sto.lvmLvPair( TestFalse<LvmLv>, TestTrue<LvmVg> );
    PrintPair<Storage::ConstLvmLvPair>( cout, p, "LvmLv impossible " );
    }
    {
    Storage::LvmLvCondIPair<StripeG1>::type p = Sto.lvmLvCondPair<StripeG1>( StripeG1() );
    PrintPair<Storage::LvmLvCondIPair<StripeG1>::type>( cout, p, "LvmLv S>1 " );
    }
    {
    Storage::LvmLvCondIPair<TestHaveA>::type p = Sto.lvmLvCondPair<TestHaveA>( TestHaveA() );
    PrintPair<Storage::LvmLvCondIPair<TestHaveA>::type>( cout, p, "LvmLv 'a' " );
    }
}
