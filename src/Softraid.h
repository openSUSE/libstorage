#ifndef SOFTRAID_H
#define SOFTRAID_H

#include <list>

using namespace std;

#include "y2storage/Container.h"

class Softraid : public Container
    {
    friend class Storage;

    public:
	Softraid();
	virtual ~Softraid();
	static CType const StaticType() { return MD; }

    protected:
    };

#endif
