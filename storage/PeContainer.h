/*
 * Copyright (c) [2004-2009] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#ifndef PE_CONTAINER_H
#define PE_CONTAINER_H

#include "storage/Container.h"
#include "storage/Dm.h"

namespace storage
{

class PeContainer : public Container
    {
    friend class Storage;

    public:

	PeContainer(Storage* s, const string& name, const string& device, CType t);
	PeContainer(Storage* s, const string& name, const string& device, CType t,
		    SystemInfo& systeminfo);
	PeContainer(Storage* s, CType t, const xmlNode* node);
	PeContainer(const PeContainer& c);
	virtual ~PeContainer();

	void saveData(xmlNode* node) const;

	unsigned long long peSize() const { return pe_size; }
	unsigned long peCount() const { return num_pe; }
	unsigned long peFree() const { return free_pe; }
	unsigned numPv() const { return pv.size(); }
	friend std::ostream& operator<< (std::ostream&, const PeContainer& );
	bool addedPv( const string& dev ) const;
	unsigned long sizeToLe( unsigned long long sizeK ) const;

	int setPeSize( long long unsigned, bool lvm1 );
	void calcSize() { size_k = pe_size * num_pe; }
	void unuseDev() const;
	void changeDeviceName( const string& old, const string& nw );

	bool equalContent( const PeContainer& rhs, bool comp_vol=true ) const;

	void logDifference(std::ostream& log, const PeContainer& rhs) const;

	string getDeviceByNumber( const string& majmin ) const;

	virtual list<string> getUsing() const;

    protected:
	struct Pv
	    {
	    Pv() : num_pe(0), free_pe(0) {}
	    Pv(const xmlNode* node);

	    void saveData(xmlNode* node) const;

	    string device;
	    string dmcryptDevice;
	    string uuid;
	    string status;
	    unsigned long num_pe;
	    unsigned long free_pe;

	    string realDevice() const { return dmcryptDevice.empty() ? device : dmcryptDevice; }

	    static bool comp_le( const Pv& a, const Pv& b )
		{ return( a.free_pe<b.free_pe ); }
	    static bool no_free( const Pv& a )
		{ return( a.free_pe==0 ); }

	    bool operator== ( const Pv& rhs ) const
		{ return( device==rhs.device ); }
	    bool operator== ( const string& dev ) const
		{ return( device==dev); }
	    bool equalContent(const Pv& rhs) const
	    {
		return device == rhs.device && dmcryptDevice == rhs.dmcryptDevice && uuid == rhs.uuid &&
		    status == rhs.status && num_pe == rhs.num_pe && free_pe == rhs.free_pe; 
	    }
	    };

	// iterators over Dm volumes
	typedef CastIterator<VIter, Dm *> DmInter;
	typedef CastIterator<CVIter, const Dm *> DmCInter;
	template< class Pred >
	    struct DmPI { typedef ContainerIter<Pred, DmInter> type; };
	template< class Pred >
	    struct DmCPI { typedef ContainerIter<Pred, DmCInter> type; };
	typedef CheckFnc<const Dm> CheckFncDm;
	typedef CheckerIterator< CheckFncDm, DmPI<CheckFncDm>::type,
				 DmInter, Dm > DmPIterator;
	typedef CheckerIterator< CheckFncDm, DmCPI<CheckFncDm>::type,
				 DmCInter, const Dm > DmCPIterator;
	typedef DerefIterator<DmPIterator,Dm> DmIter;
	typedef DerefIterator<DmCPIterator,const Dm> ConstDmIter;
	typedef IterPair<DmIter> DmPair;
	typedef IterPair<ConstDmIter> ConstDmPair;

	DmPair dmPair( bool (* Check)( const Dm& )=NULL)
	    {
	    return( DmPair( dmBegin( Check ), dmEnd( Check ) ));
	    }
	DmIter dmBegin( bool (* Check)( const Dm& )=NULL)
	    {
	    IterPair<DmInter> p( (DmInter(begin())), (DmInter(end())) );
	    return( DmIter( DmPIterator( p, Check )) );
	    }
	DmIter dmEnd( bool (* Check)( const Dm& )=NULL)
	    {
	    IterPair<DmInter> p( (DmInter(begin())), (DmInter(end())) );
	    return( DmIter( DmPIterator( p, Check, true )) );
	    }

	ConstDmPair dmPair( bool (* Check)( const Dm& )=NULL) const
	    {
	    return( ConstDmPair( dmBegin( Check ), dmEnd( Check ) ));
	    }
	ConstDmIter dmBegin( bool (* Check)( const Dm& )=NULL) const
	    {
	    IterPair<DmCInter> p( (DmCInter(begin())), (DmCInter(end())) );
	    return( ConstDmIter( DmCPIterator( p, Check )) );
	    }
	ConstDmIter dmEnd( bool (* Check)( const Dm& )=NULL) const
	    {
	    IterPair<DmCInter> p( (DmCInter(begin())), (DmCInter(end())) );
	    return( ConstDmIter( DmCPIterator( p, Check, true )) );
	    }


	friend std::ostream& operator<< (std::ostream&, const Pv& );
	friend void printDevList (std::ostream&, const std::list<Pv>& );

	string addList() const;
	virtual void print( std::ostream& s ) const { s << *this; }
	virtual Container* getCopy() const = 0; // { return( new PeContainer( *this ) ); }
	bool findPe(const string& dev, const list<Pv>& pl, list<Pv>::const_iterator& i) const;
	bool findPe(const string& dev, list<Pv>& pl, list<Pv>::iterator& i) const;
	unsigned long leByLvRemove() const;
	int tryUnusePe( const string& dev, std::list<Pv>& pl, std::list<Pv>& pladd,
	                std::list<Pv>& plrem, unsigned long& removed_pe );
	bool checkCreateConstraints();
	static int addLvPeDistribution( unsigned long le, unsigned stripe,
					std::list<Pv>& pl, std::list<Pv>& pladd, 
					std::map<string,unsigned long>& pe_map );
	int remLvPeDistribution( unsigned long le, std::map<string,unsigned long>& pe_map,
				 std::list<Pv>& pl, std::list<Pv>& pladd );
	virtual bool checkConsistency() const;

	void addPv(const Pv& p);

	unsigned long long pe_size;
	unsigned long num_pe;
	unsigned long free_pe;
	std::list<Pv> pv;
	std::list<Pv> pv_add;
	std::list<Pv> pv_remove;

    private:

	PeContainer& operator=(const PeContainer&); // disallow

    };

}

#endif
