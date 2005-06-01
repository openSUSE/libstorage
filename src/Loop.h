#ifndef LOOP_H
#define LOOP_H

#include "y2storage/Volume.h"

class Loop : public Volume
    {
    public:
	Loop( const Container& d, const string& LoopDev, const string& LoopFile );
	Loop( const Container& d, const string& file, bool reuseExisting,
	      unsigned long long sizeK );
	virtual ~Loop();
	const string& loopFile() const { return lfile; }
	void setDelFile( bool val=true ) { delFile=val; }
	bool removeFile();
	bool createFile();
	string lfileRealPath() const;
	friend inline std::ostream& operator<< (std::ostream& s, const Loop& l );

	virtual void print( std::ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;

	void getInfo( storage::LoopInfo& info ) const;

	static bool notDeleted( const Loop& l ) { return( !l.deleted() ); }

    protected:
	void init();

	string lfile;
	bool reuseFile;
	bool delFile;
    };

inline std::ostream& operator<< (std::ostream& s, const Loop& l )
    {
    s << "Loop " << *(Volume*)&l
      << " LoopFile:" << l.lfile;
    if( l.reuseFile )
      s << " reuse";
    if( l.delFile )
      s << " delFile";
    return( s );
    }


#endif
