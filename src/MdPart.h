#ifndef MD_PART_H
#define MD_PART_H

using namespace std;

#include "y2storage/Volume.h"

class Softraid;

class MdPart : public Volume
    {
    public:
	typedef enum { RAID0, RAID1, RAID5, MULTIPATH } MdType;
	MdPart( const Softraid& d, unsigned Pnr, MdType Type );
	virtual ~MdPart();
	MdType Personality() const { return md_type; }
	const string& PersonalityName() const { return md_names[md_type]; }

    protected:
	MdType md_type;
	static string md_names[MULTIPATH+1];
    };

#endif
