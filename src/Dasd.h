#ifndef DASD_H
#define DASD_H

#include "y2storage/Disk.h"

class SystemCmd;

class Dasd : public Disk
    {
    public:
	Dasd( Storage * const s, const string& Name, unsigned long long Size );
	Dasd( const Dasd& rhs );
	virtual ~Dasd();
	bool equalContent( const Dasd& rhs ) const;
	void logDifference( const Dasd& d ) const;

	friend std::ostream& operator<< (std::ostream&, const Dasd& );

    protected:
	bool detectPartitions();
	bool checkFdasdOutput( SystemCmd& Cmd );
	bool scanFdasdLine( const string& Line, unsigned& nr, 
	                    unsigned long& start, unsigned long& csize );
	void getGeometry( SystemCmd& cmd, unsigned long& c,
			  unsigned& h, unsigned& s );
	void redetectGeometry() {};
	Dasd& operator= ( const Dasd& rhs );
    };

#endif
