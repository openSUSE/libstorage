#ifndef VOLUME_H
#define VOLUME_H

using namespace std;

#include "y2storage/StorageInterface.h"
#include "y2storage/StorageTmpl.h"

class Container;
class SystemCmd;
class ProcMounts;
class EtcFstab;

class Volume
    {
    friend class Storage;

    public:
	Volume( const Container& d, unsigned Pnr, unsigned long long SizeK );
	Volume( const Container& d, const string& PName, unsigned long long SizeK );
	virtual ~Volume();

	virtual bool commitChanges() { return true; }

	const string& device() const { return dev; }
	const string& mountDevice() const { return( is_loop?loop_dev:dev ); }
	const Container* getContainer() const { return cont; }
	bool deleted() const { return del; }
	bool created() const { return create; }
	void setDeleted( bool val=true ) { del=val; }
	void setCreated( bool val=true ) { create=val; }
	bool loop() const { return is_loop; }
	bool noauto() const { return !mauto; }
	string getUuid() const { return uuid; }
	string getLabel() const { return label; }
	bool setLabel( string val );
	storage::EncryptType getEncryption() const { return encryption; }
	void setEncryption( storage::EncryptType val=storage::ENC_TWOFISH ) 
	    { encryption=val; }
	string getCryptPwd() const { return crypt_pwd; }
	void setCryptPwd( string val ) { crypt_pwd=val; }
	string getMount() const { return mount; }
	void setMount( string val ) { mount=val; }
	storage::FsType getFs() const { return fs; }
	void setFs( storage::FsType val ) { fs=val; }
	storage::MountByType getMountBy() const { return mount_by; }
	void setMountBy( storage::MountByType val ) { mount_by=val; }
	string getFstabOption() const { return fstab_opt; }
	void setFstabOption( string val ) { fstab_opt=val; }
	string getMkfsOption() const { return mkfs_opt; }
	void setMkfsOption( string val ) { mkfs_opt=val; }
	const list<string>& altNames() const { return( alt_names ); }
	unsigned nr() const { return num; }
	unsigned long long sizeK() const { return size_k; }
	const string& name() const { return nm; }
	unsigned long minorNumber() const { return mnr; }
	unsigned long majorNumber() const { return mjr; }
	void setMajorMinor( unsigned long Major, unsigned long Minor )
	    { mjr=Major; mnr=Minor; }
	void setSize( unsigned long long SizeK ) { size_k=SizeK; }

        bool operator== ( const Volume& rhs ) const;
        bool operator!= ( const Volume& rhs ) const
            { return( !(*this==rhs) ); }
        bool operator< ( const Volume& rhs ) const;
        bool operator<= ( const Volume& rhs ) const
            { return( *this<rhs || *this==rhs ); }
        bool operator>= ( const Volume& rhs ) const
            { return( !(*this<rhs) ); }
        bool operator> ( const Volume& rhs ) const
            { return( !(*this<=rhs) ); }
	friend ostream& operator<< (ostream& s, const Volume &v );

	struct SkipDeleted
	    {
	    bool operator()(const Volume&d) const { return( !d.deleted());}
	    };
	static SkipDeleted SkipDel;
	static bool notDeleted( const Volume&d ) { return( !d.deleted() ); }
	static bool getMajorMinor( const string& device, 
	                           unsigned long& Major, unsigned long& Minor );
	static storage::EncryptType toEncType( const string& val );

    protected:
	void init();
	void setNameDev();
	void getFsData( SystemCmd& blkidData );
	void getLoopData( SystemCmd& loopData );
	void getMountData( const ProcMounts& mountData );
	void getFstabData( const EtcFstab& mountData );

	const Container* const cont;
	bool numeric;
	bool create;
	bool del;
	bool format;
	bool mauto;
	storage::FsType fs;
	storage::FsType detected_fs;
	storage::MountByType mount_by;
	storage::MountByType orig_mount_by;
	string uuid;
	string label;
	string orig_label;
	string mount;
	string orig_mount;
	string fstab_opt;
	string orig_fstab_opt;
	string mkfs_opt;
	bool is_loop;
	storage::EncryptType encryption;
	string loop_dev;
	string fstab_loop_dev;
	string crypt_pwd;
	string nm;
	list<string> alt_names;
	unsigned num;
	unsigned long long size_k;
	string dev;
	unsigned long mnr;
	unsigned long mjr;

	static string fs_names[storage::SWAP+1];
	static string mb_names[storage::MOUNTBY_LABEL+1];
	static string enc_names[storage::ENC_UNKNOWN+1];
    };

inline ostream& operator<< (ostream& s, const Volume &v )
    {
    s << "Device:" << v.dev;
    if( v.numeric )
	s << " Nr:" << v.num;
    else
	s << " Name:" << v.nm;
    s << " SizeK:" << v.size_k 
	<< " Node <" << v.mjr
	<< ":" << v.mnr << ">";
    if( v.del )
	s << " deleted";
    if( v.create )
	s << " created";
    if( v.format )
	s << " format";
    if( v.fs != storage::UNKNOWN )
	{
	s << " fs:" << Volume::fs_names[v.fs];
	if( v.fs != v.detected_fs && v.detected_fs!=storage::UNKNOWN )
	    s << " det_fs:" << Volume::fs_names[v.detected_fs];
	}
    if( v.mount.length()>0 )
	{
	s << " mount:" << v.mount;
	if( v.mount != v.orig_mount && v.orig_mount.length()>0 )
	    s << " orig_mount:" << v.orig_mount;
	}
    if( !v.mauto )
	s << " noauto";
    if( v.mount_by != storage::MOUNTBY_DEVICE )
	{
	s << " mount_by:" << Volume::mb_names[v.mount_by];
	if( v.mount_by != v.orig_mount_by )
	    s << " orig_mount_by:" << Volume::mb_names[v.orig_mount_by];
	}
    if( v.uuid.length()>0 )
	{
	s << " uuid:" << v.uuid;
	}
    if( v.label.length()>0 )
	{
	s << " label:" << v.label;
	if( v.label != v.orig_label && v.orig_label.length()>0 )
	    s << " orig_label:" << v.orig_label;
	}
    if( v.fstab_opt.length()>0 )
	{
	s << " fstopt:" << v.fstab_opt;
	if( v.fstab_opt != v.orig_fstab_opt && v.orig_fstab_opt.length()>0 )
	    s << " orig_fstopt:" << v.orig_fstab_opt;
	}
    if( v.mkfs_opt.length()>0 )
	{
	s << " mkfsopt:" << v.mkfs_opt;
	}
    if( v.alt_names.begin() != v.alt_names.end() )
	{
	s << " alt_names:" << v.alt_names;
	}
    if( v.is_loop )
	{
	s << " loop:" << v.loop_dev;
	if( v.fstab_loop_dev != v.loop_dev )
	    {
	    s << " fstab_loop:" << v.fstab_loop_dev;
	    }
	s << " encr:" << v.enc_names[v.encryption];
#ifdef DEBUG_LOOP_CRYPT_PASSWORD
	s << " pwd:" << v.crypt_pwd;
#endif
	}
    return( s );
    }

#endif
