#ifndef LOOP_H
#define LOOP_H

using namespace std;

#include "y2storage/Volume.h"

class Loop : public Volume
    {
    public:
	Loop( const Container& d, const string& LoopDev, const string& LoopFile );
	virtual ~Loop();
	const string& loopFile() const { return lfile; }
	friend inline ostream& operator<< (ostream& s, const Loop& l );

	virtual void print( ostream& s ) const { s << *this; }
	string removeText( bool doing ) const;
	string createText( bool doing ) const;
	string formatText( bool doing ) const;

    protected:
	string lfile;
    };

inline ostream& operator<< (ostream& s, const Loop& l )
    {
    s << "Loop " << *(Volume*)&l
      << " LoopFile:" << l.lfile;
    return( s );
    }


#endif
