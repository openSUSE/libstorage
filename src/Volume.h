#ifndef VOLUME_H
#define VOLUME_H

using namespace std;

class Container;

class Volume
    {
    public:
	Volume( const Container& d, unsigned Pnr );
	Volume( const Container& d, const string& PName );
	virtual ~Volume();
	const string& Device() const { return dev; }; 
	bool Delete() const { return deleted; }
	unsigned Nr() const { return nr; }
	const string& Name() const { return name; }

    protected:
	void Init();

	const Container* const cont;
	bool numeric;
	bool deleted;
	string name;
	unsigned nr;
	string dev;
    };

#endif
