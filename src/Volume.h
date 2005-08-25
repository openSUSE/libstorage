#ifndef VOLUME_H
#define VOLUME_H

#include "y2storage/StorageInterface.h"
#include "y2storage/StorageTypes.h"
#include "y2storage/StorageTmpl.h"

class SystemCmd;
class ProcMounts;
class EtcFstab;
class FstabEntry;
class Container;
class Storage;

class Volume
    {
    friend class Storage;

    public:
	Volume( const Container& d, unsigned Pnr, unsigned long long SizeK );
	Volume( const Container& d, const string& PName, unsigned long long SizeK );
	Volume( const Container& d );
	Volume( const Volume& );
	Volume& operator=( const Volume& );

	virtual ~Volume();

	const string& device() const { return dev; }
	const string& mountDevice() const { return( is_loop?loop_dev:dev ); }
	const string& loopDevice() const { return( loop_dev ); }
	const Container* getContainer() const { return cont; }
	storage::CType cType() const;
	bool deleted() const { return del; }
	bool created() const { return create; }
	void setDeleted( bool val=true ) { del=val; }
	void setCreated( bool val=true ) { create=val; }
	void setReadonly( bool val=true ) { ronly=val; }
	bool ignoreFstab() const { return( ignore_fstab ); }
	void setIgnoreFstab( bool val=true ) { ignore_fstab=val; }
	void setFstabAdded( bool val=true ) { fstab_added=val; }
	bool fstabAdded() const { return( fstab_added ); }
	const storage::usedBy& getUsedBy()  const{ return( uby ); }
	storage::UsedByType getUsedByType() const { return( uby.type() ); }
	const string& usedByName() const { return( uby.name() ); }
	void setUsedBy( storage::UsedByType t, const string& name ) { uby.set( t, name );}

	virtual int setFormat( bool format=true, storage::FsType fs=storage::REISERFS );
	void formattingDone() { format=false; detected_fs=fs; }
	bool getFormat() const { return format; }
	int changeFstabOptions( const string& options );
	int changeMountBy( storage::MountByType mby );
	virtual int changeMount( const string& m );
	bool loop() const { return is_loop; }
	bool loopActive() const { return( is_loop&&loop_active ); }
	bool needLosetup() const { return is_loop!=loop_active; }
	const string& getUuid() const { return uuid; }
	const string& getLabel() const { return label; }
	int setLabel( const string& val ); 
	bool needLabel() const { return( label!=orig_label ); }
	storage::EncryptType getEncryption() const { return encryption; }
	void setEncryption( storage::EncryptType val=storage::ENC_TWOFISH )
	    { encryption=orig_encryption=val; }
	int setEncryption( bool val );
	const string& getCryptPwd() const { return crypt_pwd; }
	int setCryptPwd( const string& val ); 
	void clearCryptPwd() { crypt_pwd.erase(); } 
	const string& getMount() const { return mp; }
	bool needRemount() const; 
	bool needShrink() const { return(size_k<orig_size_k); }
	bool needExtend() const { return(size_k>orig_size_k); }
	long long extendSize() const { return(orig_size_k-size_k);}
	storage::FsType getFs() const { return fs; }
	void setFs( storage::FsType val ) { detected_fs=fs=val; }
	storage::MountByType getMountBy() const { return mount_by; }
	const string& getFstabOption() const { return fstab_opt; }
	void setFstabOption( const string& val ) { fstab_opt=val; }
	bool needFstabUpdate() const
	    { return( !ignore_fstab && 
	              (fstab_opt!=orig_fstab_opt || mount_by!=orig_mount_by ||
	               encryption!=orig_encryption) ); }
	const string& getMkfsOption() const { return mkfs_opt; }
	int setMkfsOption( const string& val ) { mkfs_opt=val; return 0; }
	const std::list<string>& altNames() const { return( alt_names ); }
	unsigned nr() const { return num; }
	unsigned long long sizeK() const { return size_k; }
	unsigned long long origSizeK() const { return orig_size_k; }
	const string& name() const { return nm; }
	unsigned long minorNr() const { return mnr; }
	unsigned long majorNr() const { return mjr; }
	void setMajorMinor( unsigned long Major, unsigned long Minor )
	    { mjr=Major; mnr=Minor; }
	void setSize( unsigned long long SizeK ) { size_k=orig_size_k=SizeK; }
	virtual void setResizedSize( unsigned long long SizeK ) { size_k=SizeK; }
	virtual void forgetResize() { size_k=orig_size_k; }
	virtual bool canUseDevice() const;

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
	bool equalContent( const Volume& rhs ) const;
	string logDifference( const Volume& c ) const;
	friend std::ostream& operator<< (std::ostream& s, const Volume &v );

	int prepareRemove();
	int umount( const string& mp="" );
	int loUnsetup();
	int mount( const string& mp="" );
	int canResize( unsigned long long newSizeK ) const;
	int doMount();
	int doFormat();
	int doLosetup();
	int doSetLabel();
	int doFstabUpdate();
	int resizeFs();
	void fstabUpdateDone();
	bool isMounted() const { return( is_mounted ); }
	virtual string removeText(bool doing=true) const;
	virtual string createText(bool doing=true) const;
	virtual string resizeText(bool doing=true) const; 
	virtual string formatText(bool doing=true) const;
	virtual void getCommitActions( std::list<storage::commitAction*>& l ) const;
	string mountText( bool doing=true ) const;
	string labelText( bool doing=true ) const;
	string losetupText( bool doing=true ) const;
	string fstabUpdateText() const;
	string sizeString() const;
	string bootMount() const;
	bool optNoauto() const;
	bool inCrypto() const { return( is_loop && !optNoauto() ); }
	virtual void print( std::ostream& s ) const { s << *this; }
	int getFreeLoop();
	void getInfo( storage::VolumeInfo& info ) const;
	void mergeFstabInfo( storage::VolumeInfo& tinfo, const FstabEntry& fste ) const;
	static bool loopInUse( Storage* sto, const string& loopdev );

	struct SkipDeleted
	    {
	    bool operator()(const Volume&d) const { return( !d.deleted());}
	    };
	static SkipDeleted SkipDel;
	static bool notDeleted( const Volume&d ) { return( !d.deleted() ); }
	static bool isDeleted( const Volume&d ) { return( d.deleted() ); }
	static bool getMajorMinor( const string& device,
	                           unsigned long& Major, unsigned long& Minor );
	static bool loopStringNum( const string& name, unsigned& num );
	static storage::EncryptType toEncType( const string& val );
	static storage::FsType toFsType( const string& val );
	static storage::MountByType toMountByType( const string& val );
	const string& fsTypeString() const { return fs_names[fs]; }
	static const string& fsTypeString( const storage::FsType type )
	    { return fs_names[type]; }
	static const string& encTypeString( const storage::EncryptType type )
	    { return enc_names[type]; }
	static const string& mbyTypeString( const storage::MountByType type )
	    { return mb_names[type]; }


    protected:
	void init();
	void setNameDev();
	int checkDevice();
	int checkDevice( const string& device );
	void getFsData( SystemCmd& blkidData );
	void getLoopData( SystemCmd& loopData );
	void getMountData( const ProcMounts& mountData );
	void getFstabData( EtcFstab& fstabData );
	void getTestmodeData( const string& data );
	string getMountByString( storage::MountByType mby, const string& dev,
	                         const string& uuid, const string& label ) const;
	void setExtError( const SystemCmd& cmd, bool serr=true );

	std::ostream& logVolume( std::ostream& file ) const;
	string getLosetupCmd( storage::EncryptType e, const string& pwdfile ) const;
	storage::EncryptType detectLoopEncryption();

	const Container* const cont;
	bool numeric;
	bool create;
	bool del;
	bool format;
	bool silent;
	bool fstab_added;
	storage::FsType fs;
	storage::FsType detected_fs;
	storage::MountByType mount_by;
	storage::MountByType orig_mount_by;
	string uuid;
	string label;
	string orig_label;
	string mp;
	string orig_mp;
	string fstab_opt;
	string orig_fstab_opt;
	string mkfs_opt;
	bool is_loop;
	bool is_mounted;
	bool ignore_fstab;
	bool loop_active;
	bool ronly;
	storage::EncryptType encryption;
	storage::EncryptType orig_encryption;
	string loop_dev;
	string fstab_loop_dev;
	string crypt_pwd;
	string nm;
	std::list<string> alt_names;
	unsigned num;
	unsigned long long size_k;
	unsigned long long orig_size_k;
	string dev;
	unsigned long mnr;
	unsigned long mjr;
	storage::usedBy uby;

	static string fs_names[storage::FSNONE+1];
	static string mb_names[storage::MOUNTBY_LABEL+1];
	static string enc_names[storage::ENC_UNKNOWN+1];

	mutable storage::VolumeInfo info;
    };

#endif
