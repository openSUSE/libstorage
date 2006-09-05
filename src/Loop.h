#ifndef LOOP_H
#define LOOP_H

#include "y2storage/Volume.h"

namespace storage
{
class LoopCo;
class ProcPart;

class Loop : public Volume
    {
    public:
	Loop( const LoopCo& d, const string& LoopDev, const string& LoopFile,
	      ProcPart& ppart );
	Loop( const LoopCo& d, const string& file, bool reuseExisting,
	      unsigned long long sizeK );
	Loop( const LoopCo& d, const Loop& d );
	virtual ~Loop();
	const string& loopFile() const { return lfile; }
	void setLoopFile( const string& file );
	bool getReuse() { return( reuseFile ); }
	void setReuse( bool reuseExisting );
	void setDelFile( bool val=true ) { delFile=val; }
	bool removeFile();
	bool createFile();
	string lfileRealPath() const;
	friend std::ostream& operator<< (std::ostream& s, const Loop& l );

	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;

	void getInfo( storage::LoopInfo& info ) const;
	bool equalContent( const Loop& rhs ) const;
	void logDifference( const Loop& d ) const;
	static unsigned loopMajor();

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
	mutable storage::LoopInfo info;
    };

}

#endif
