#ifndef MD_H
#define MD_H

using namespace std;

#include "y2storage/Volume.h"

class Md : public Volume
    {
    public:
	typedef enum { RAID0, RAID1, RAID5, MULTIPATH } MdType;
	Md( const Container& d, unsigned Pnr, MdType Type );
	virtual ~Md();
	MdType Personality() const { return md_type; }
	const string& PersonalityName() const { return md_names[md_type]; }
	friend inline ostream& operator<< (ostream& s, const Md& m );


    protected:
	MdType md_type;
	static string md_names[MULTIPATH+1];
    };

inline ostream& operator<< (ostream& s, const Md& m )
    {
    s << "Md " << Volume(m)
      << " Personality:" << m.PersonalityName();
    return( s );
    }


#endif
