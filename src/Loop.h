#ifndef LOOP_H
#define LOOP_H

using namespace std;

#include "y2storage/Volume.h"

class Loop : public Volume
    {
    public:
	Loop( const Container& d, unsigned Pnr, const string& LoopFile );
	virtual ~Loop();
	const string& LoopFile() const { return loop_file; }
	friend inline ostream& operator<< (ostream& s, const Loop& l );

    protected:
	string loop_file;
    };

inline ostream& operator<< (ostream& s, const Loop& l )
    {
    s << "Loop " << Volume(l) 
      << " LoopFile:" << l.loop_file;
    return( s );
    }


#endif
