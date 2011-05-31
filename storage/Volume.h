/*
 * Copyright (c) [2004-2010] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#ifndef VOLUME_H
#define VOLUME_H

#include "storage/Blkid.h"
#include "storage/StorageInterface.h"
#include "storage/StorageTypes.h"
#include "storage/StorageTmpl.h"
#include "storage/Device.h"
#include "storage/Enum.h"


namespace storage
{
    using std::list;


class SystemCmd;
class ProcMounts;
class EtcFstab;
class FstabEntry;
class Container;
class Storage;
    

    class Volume : public Device
    {
    friend class Storage;

    public:

	Volume(const Container& c, unsigned Pnr, unsigned long long SizeK);
	Volume(const Container& c, const string& name, const string& device);
	Volume(const Container& c, const string& name, const string& device,
	       SystemInfo& systeminfo);
	Volume(const Container& c, const xmlNode* node);
	Volume(const Container& c, const Volume& v);
	virtual ~Volume();

	void saveData(xmlNode* node) const;

	const string& mountDevice() const;
	const string& loopDevice() const { return( loop_dev ); }
	const string& dmcryptDevice() const { return( dmcrypt_dev ); }

	const Container* getContainer() const { return cont; }
	Storage* getStorage() const;

	storage::CType cType() const;
	void setReadonly( bool val=true ) { ronly=val; }
	bool ignoreFstab() const { return( ignore_fstab ); }
	void setIgnoreFstab( bool val=true ) { ignore_fstab=val; }
	bool ignoreFs() const { return( ignore_fs ); }
	void setIgnoreFs( bool val=true ) { ignore_fs=val; }
	void setFstabAdded( bool val=true ) { fstab_added=val; }
	bool fstabAdded() const { return( fstab_added ); }

	void getFsInfo( const Volume* source );

	virtual int setFormat( bool format=true, storage::FsType fs=storage::REISERFS );
	void formattingDone() { format=false; fs=detected_fs; }
	bool getFormat() const { return format; }
	int changeFstabOptions( const string& options );
	int changeMountBy( storage::MountByType mby );
	virtual int changeMount( const string& m );
	bool loop() const { return is_loop; }
	bool dmcrypt() const { return encryption != ENC_NONE && encryption != ENC_UNKNOWN; }
	bool loopActive() const { return( is_loop&&loop_active ); }
	bool dmcryptActive() const { return( dmcrypt()&&dmcrypt_active ); }
	bool needCrsetup( bool urgent=true ) const; 
	const string& getUuid() const { return uuid; }
	const string& getLabel() const { return label; }
	int setLabel( const string& val );
	int eraseLabel() { label.erase(); orig_label.erase(); return 0; }
	int eraseUuid() { uuid.erase(); orig_uuid.erase(); return 0; }
	bool needLabel() const { return( label!=orig_label ); }
	storage::EncryptType getEncryption() const { return encryption; }
	void initEncryption( storage::EncryptType val=storage::ENC_LUKS )
	    { encryption=orig_encryption=val; }
	virtual int setEncryption(bool val, storage::EncryptType typ = storage::ENC_LUKS );
	const string& getCryptPwd() const { return crypt_pwd; }
	int setCryptPwd( const string& val );
	void clearCryptPwd() { crypt_pwd.erase(); orig_crypt_pwd.erase(); }
	bool needCryptPwd() const; 
	const string& getMount() const { return mp; }
	bool hasOrigMount() const { return !orig_mp.empty(); }
	bool needRemount() const;
	bool needShrink() const { return !del && size_k<orig_size_k; }
	bool needExtend() const { return !del && size_k>orig_size_k; }
	long long extendSize() const { return size_k-orig_size_k; }
	storage::FsType getFs() const { return fs; }
	storage::FsType detectedFs() const { return detected_fs; }
	void setFs( storage::FsType val ) { detected_fs=fs=val; }
	void initUuid( const string& id ) { uuid=orig_uuid=id; }
	void initLabel( const string& lbl ) { label=orig_label=lbl; }
	storage::MountByType getMountBy() const { return mount_by; }
	const string& getFstabOption() const { return fstab_opt; }
	void setFstabOption( const string& val ) { orig_fstab_opt=fstab_opt=val; }
	unsigned fstabFreq() const;
	unsigned fstabPassno() const;
	void setMount( const string& val ) { orig_mp=mp=val; }
	void updateFstabOptions();
	bool needFstabUpdate() const;
	const string& getMkfsOption() const { return mkfs_opt; }
	int setMkfsOption( const string& val ) { mkfs_opt=val; return 0; }
	const string& getTunefsOption() const { return tunefs_opt; }
	int setTunefsOption( const string& val ) { tunefs_opt=val; return 0; }
	const string& getDescText() const { return dtxt; }
	int setDescText( const string& val ) { dtxt=val; return 0; }
	unsigned nr() const { return num; }
	unsigned long long origSizeK() const { return orig_size_k; }
	bool isNumeric() const { return numeric; }
	void setSize( unsigned long long SizeK ) { size_k=orig_size_k=SizeK; }
	virtual void setResizedSize( unsigned long long SizeK ) { size_k=SizeK; }
	void setUsedByUuid( UsedByType type, const string& uuid );
	void setDmcryptDev( const string& dm, bool active );
	void setDmcryptDevEnc( const string& dm, storage::EncryptType typ, bool active );
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

	bool equalContent(const Volume& rhs) const;

	void logDifference(std::ostream& log, const Volume& rhs) const;

	friend std::ostream& operator<< (std::ostream& s, const Volume &v );

	int prepareRemove();
	int unaccessVol();
	int umount( const string& mp="" );
	int crUnsetup( bool force=false );
	int mount( const string& mp="", bool ro=false );
	int canResize( unsigned long long newSizeK ) const;
	int doMount();
	int doFormat();
	int doCrsetup();
	int doSetLabel();
	int doFstabUpdate(bool force=false);
	int resizeFs();
	void fstabUpdateDone();
	bool isMounted() const { return( is_mounted ); }
	virtual Text removeText(bool doing) const;
	virtual Text createText(bool doing) const;
	virtual Text resizeText(bool doing) const;
	virtual Text formatText(bool doing) const;
	virtual void getCommitActions(list<commitAction>& l) const;
	virtual Text mountText(bool doing) const;
	Text labelText(bool doing) const;
	Text losetupText(bool doing) const;
	Text crsetupText(bool doing) const;
	Text fstabUpdateText() const;
	bool optNoauto() const;
	bool inCryptotab() const { return( encryption!=ENC_NONE && 
	                                   encryption!=ENC_LUKS && 
					   !optNoauto() ); }
	bool inCrypttab() const { return( encryption==ENC_LUKS ); }
	bool pvEncryption() const;
	virtual void print( std::ostream& s ) const { s << *this; }
	int getFreeLoop();
	int getFreeLoop( SystemCmd& loopData );
	int getFreeLoop( SystemCmd& loopData, const std::list<unsigned>& ids );
	void getInfo( storage::VolumeInfo& info ) const;
	void mergeFstabInfo( storage::VolumeInfo& tinfo, const FstabEntry& fste ) const;
	void updateFsData( bool setUsedByLvm=false );
	void triggerUdevUpdate() const;
	static bool loopInUse(const Storage* sto, const string& loopdev);

	static bool notDeleted( const Volume&d ) { return( !d.deleted() ); }
	static bool isDeleted( const Volume&d ) { return( d.deleted() ); }

	static bool loopStringNum( const string& name, unsigned& num );

	const string& fsTypeString() const { return toString(fs); }

	static bool isTmpCryptMp( const string& mp );

    protected:
	void init();
	void setNameDev();
	int checkDevice() const;
	int checkDevice(const string& device) const;
	MountByType defaultMountBy() const;
	bool allowedMountBy(MountByType mby) const;
	bool findBlkid( const Blkid& blkid, Blkid::Entry& entry );
	void getFsData(const Blkid& blkid, bool setUsedByLvm=false );
	void getLoopData( SystemCmd& loopData );
	void getMountData(const ProcMounts& mounts, bool swap_only = false);
	void getFstabData( EtcFstab& fstabData );
	void replaceAltName( const string& prefix, const string& newn );
	string getMountByString() const;
	string getFstabDevice() const;
	string getFstabDentry() const;
	list<string> getFstabOpts() const;
	bool getLoopFile( string& fname ) const;
	void setExtError( const SystemCmd& cmd, bool serr=true );
	string getDmcryptName() const;
	bool needLosetup( bool urgent ) const; 
	bool needCryptsetup() const; 
	int doLosetup();
	int doCryptsetup();
	int loUnsetup( bool force=false );
	int cryptUnsetup( bool force=false );
	bool pwdLengthOk( storage::EncryptType typ, const string& val, 
	                  bool format ) const;
	bool noFreqPassno() const;
	int prepareTmpMount( string& m, bool& needUmount );
	int umountTmpMount( int ret );
	int doFormatBtrfs();

	string getLosetupCmd( storage::EncryptType, const string& pwdfile ) const;
	string getCryptsetupCmd( storage::EncryptType e, const string& dmdev,
				 const string& mp, const string& pwdfile, bool format,
				 bool empty_pwd=false ) const;
	storage::EncryptType detectEncryption();
	string getFilesysSysfsPath() const;

	const Container* const cont;
	bool numeric;
	bool format;
	bool fstab_added;
	storage::FsType fs;
	storage::FsType detected_fs;
	storage::MountByType mount_by;
	storage::MountByType orig_mount_by;
	string uuid;
	string orig_uuid;
	string label;
	string orig_label;
	string mp;
	string orig_mp;
	string fstab_opt;
	string orig_fstab_opt;
	string mkfs_opt;
	string tunefs_opt;
	bool is_loop;
	bool is_mounted;
	bool ignore_fstab;
	bool ignore_fs;
	bool loop_active;
	bool dmcrypt_active;
	bool ronly;
	storage::EncryptType encryption;
	storage::EncryptType orig_encryption;
	string loop_dev;
	string dmcrypt_dev;
	string fstab_loop_dev;
	string crypt_pwd;
	string orig_crypt_pwd;
	unsigned num;
	unsigned long long orig_size_k;
	string dtxt;

	static const string tmp_mount[3];

	mutable storage::VolumeInfo info; // workaround for broken ycp bindings

    private:

	Volume(const Volume&);		  // disallow
	Volume& operator=(const Volume&); // disallow

    };

}

#endif
