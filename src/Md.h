#ifndef MD_H
#define MD_H

using namespace std;

#include "y2storage/StorageInterface.h"
#include "y2storage/Volume.h"

using namespace storage;


class Md : public Volume
    {
    public:

	Md( const Container& d, unsigned Pnr, MdType Type );
	virtual ~Md();
	MdType personality() const { return md_type; }
	const string& personalityName() const { return md_names[md_type]; }
	friend inline ostream& operator<< (ostream& s, const Md& m );

    protected:
	MdType md_type;
	static string md_names[MULTIPATH+1];
    };

inline ostream& operator<< (ostream& s, const Md& m )
    {
    s << "Md " << Volume(m)
      << " Personality:" << m.personalityName();
    return( s );
    }


#endif
