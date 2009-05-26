#ifndef LOOP_H
#define LOOP_H

#include "storage/Volume.h"

namespace storage
{
class LoopCo;
class ProcPart;

class Loop : public Volume
    {
    public:
	Loop( const LoopCo& d, const string& LoopDev, const string& LoopFile,
	      bool dmcrypt, const string& dm_dev,
	      ProcPart& ppart, SystemCmd& losetup );
	Loop( const LoopCo& d, const string& file, bool reuseExisting,
	      unsigned long long sizeK, bool dmcr );
	Loop( const LoopCo& d, const Loop& rhs );
	virtual ~Loop();
	const string& loopFile() const { return lfile; }
	void setLoopFile( const string& file );
	bool getReuse() { return( reuseFile ); }
	void setReuse( bool reuseExisting );
	void setDelFile( bool val=true ) { delFile=val; }
	bool removeFile();
	bool createFile();
	string lfileRealPath() const;
	void setDmcryptDev( const string& dm_dev, bool active=true );
	friend std::ostream& operator<< (std::ostream& s, const Loop& l );

	virtual void print( std::ostream& s ) const { s << *this; }
	virtual int setEncryption( bool val, storage::EncryptType typ=storage::ENC_LUKS );

	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;

	void getInfo( storage::LoopInfo& info ) const;
	bool equalContent( const Loop& rhs ) const;
	void logDifference( const Loop& d ) const;
	static unsigned major();
	static string loopDeviceName( unsigned num );

	static bool notDeleted( const Loop& l ) { return( !l.deleted() ); }

    protected:
	void init();
	void checkReuse();
	static void getLoopMajor();
	Loop& operator=( const Loop& );

	string lfile;
	bool reuseFile;
	bool delFile;

	static unsigned loop_major;

	mutable storage::LoopInfo info; // workaround for broken ycp bindings
    };

}

#endif
