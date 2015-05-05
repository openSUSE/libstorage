/*
 * Copyright (c) [2004-2013] Novell, Inc.
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


#ifndef STORAGE_TMPL_H
#define STORAGE_TMPL_H

#include <functional>
#include <ostream>
#include <fstream>
#include <sstream>
#include <list>
#include <map>
#include <deque>
#include <set>

#include "storage/IterPair.h"
#include "storage/FilterIterator.h"
#include "storage/DerefIterator.h"
#include "storage/Utils/AppUtil.h"

namespace storage
{

    template< class Value >
    class CheckFnc
    {
    public:
        CheckFnc( bool (* ChkFnc)( Value& )=NULL ) : m_check(ChkFnc) {}
	bool operator()(Value& d) const
	    { return(m_check==NULL || (*m_check)(d)); }
    private:
	bool (* m_check)( Value& d );
    };

    template< class Pred, class Iter >
    class ContainerIter : public FilterIterator< Pred, Iter >
    {
	typedef FilterIterator< Pred, Iter > _bclass;
    public:
	ContainerIter() : _bclass() {}
	ContainerIter( const Iter& b, const Iter& e, const Pred& p,
		       bool atend=false ) :
	    _bclass(b, e, p, atend ) {}
	ContainerIter( const IterPair<Iter>& pair, const Pred& p,
		       bool atend=false ) :
	    _bclass(pair, p, atend ) {}
	template< class It >
	ContainerIter( const It& i) : _bclass( i.begin(), i.end(), i.pred() )
	    { this->m_cur=i.cur(); }
    };

    template< class Pred, class Iter, class Value >
    class ContainerDerIter : public DerefIterator<Iter,Value>
    {
	typedef DerefIterator<Iter,Value> _bclass;
    public:
	ContainerDerIter() : _bclass() {}
	ContainerDerIter( const _bclass& i ) : _bclass(i) {}
	template< class It >
	ContainerDerIter( const It& i) : _bclass( i )
	    { this->m_cur=i.cur(); }
    };

    template< class Iter, class CastResult >
    class CastIterator : public Iter
    {
    public:
	typedef CastResult value_type;
	typedef CastResult& reference;
	typedef CastResult* pointer;

	CastIterator() : Iter() {}
	CastIterator( const Iter& i ) : Iter( i ) {}
	template< class It >
	CastIterator( const It& i ) : Iter( i ) {}
	CastResult operator*() const
	{
	    return( static_cast<CastResult>(Iter::operator*()) );
	}
	CastResult* operator->() const
	{
	    return( static_cast<CastResult*>(Iter::operator->()) );
	}
	CastIterator& operator++() 
	{ 
	    Iter::operator++(); return(*this); 
	}
	CastIterator operator++(int) 
	{ 
	    y2war( "Expensive ++ CastIterator" );
	    CastIterator tmp(*this);
	    Iter::operator++(); 
	    return(tmp); 
	}
	CastIterator& operator--() 
	{ 
	    Iter::operator--(); return(*this); 
	}
	CastIterator operator--(int) 
	{ 
	    y2war( "Expensive -- CastIterator" );
	    CastIterator tmp(*this);
	    Iter::operator--(); 
	    return(tmp); 
	}
    };

    template < class Checker, class ContIter, class Iter, class Value >
    class CheckerIterator : public ContIter
    {
    public:
	CheckerIterator() {};
	CheckerIterator( const Iter& b, const Iter& e,
			 bool (* CheckFnc)( const Value& )=NULL,
			 bool atend=false ) :
	    ContIter( b, e, Checker( CheckFnc ), atend ) {}
	CheckerIterator( const IterPair<Iter>& p, 
			 bool (* CheckFnc)( const Value& )=NULL,
			 bool atend=false ) :
	    ContIter( p, Checker( CheckFnc ), atend ) {}
	template<class It>
	CheckerIterator( const It& i ) :
	    ContIter( i.begin(), i.end(), Checker(i.pred()), false )
	    { this->m_cur=i.cur(); }
    };


    template<class Num> string decString(Num number)
    {
	static_assert(std::is_integral<Num>::value, "not integral");

	std::ostringstream num_str;
	classic(num_str);
	num_str << number;
	return num_str.str();
    }

    template<class Num> string hexString(Num number)
    {
	static_assert(std::is_integral<Num>::value, "not integral");

	std::ostringstream num_str;
	classic(num_str);
	num_str << std::hex << number;
	return num_str.str();
    }

    template<class Value> void operator>>(const string& d, Value& v)
    {
	std::istringstream Data(d);
	classic(Data);
	Data >> v;
    }

    template<class Value> std::ostream& operator<<( std::ostream& s, const std::list<Value>& l )
    {
	s << "<";
	for( typename std::list<Value>::const_iterator i=l.begin(); i!=l.end(); i++ )
	{
	    if( i!=l.begin() )
		s << " ";
	    s << *i;
	}
	s << ">";
	return( s );
    }

    template<class Value> std::ostream& operator<<( std::ostream& s, const std::deque<Value>& l )
    {
	s << "<";
	for( typename std::deque<Value>::const_iterator i=l.begin(); i!=l.end(); i++ )
	{
	    if( i!=l.begin() )
		s << " ";
	    s << *i;
	}
	s << ">";
	return( s );
    }


    template<class Value>
    std::ostream& operator<<(std::ostream& s, const std::set<Value>& l)
    {
	s << "<";
	for (typename std::set<Value>::const_iterator it = l.begin(); it != l.end(); ++it)
	{
	    if (it != l.begin())
		s << " ";
	    s << *it;
	}
	s << ">";
	return s;
    }


    template<class F, class S> std::ostream& operator<<( std::ostream& s, const std::pair<F,S>& p )
    {
	s << "[" << p.first << ":" << p.second << "]";
	return( s );
    }

    template<class Key, class Value> std::ostream& operator<<( std::ostream& s, const std::map<Key,Value>& m )
    {
	s << "<";
	for( typename std::map<Key,Value>::const_iterator i=m.begin(); i!=m.end(); i++ )
	{
	    if( i!=m.begin() )
		s << " ";
	    s << i->first << ":" << i->second;
	}
	s << ">";
	return( s );
    }


    template <typename Type>
    void logDiff(std::ostream& log, const char* text, const Type& lhs, const Type& rhs)
    {
	static_assert(!std::is_enum<Type>::value, "is enum");

	if (lhs != rhs)
	    log << " " << text << ":" << lhs << "-->" << rhs;
    }

    template <typename Type>
    void logDiffHex(std::ostream& log, const char* text, const Type& lhs, const Type& rhs)
    {
	static_assert(std::is_integral<Type>::value, "not integral");

	if (lhs != rhs)
	    log << " " << text << ":" << std::hex << lhs << "-->" << rhs << std::dec;
    }

    template <typename Type>
    void logDiffEnum(std::ostream& log, const char* text, const Type& lhs, const Type& rhs)
    {
	static_assert(std::is_enum<Type>::value, "not enum");

	if (lhs != rhs)
	    log << " " << text << ":" << toString(lhs) << "-->" << toString(rhs);
    }

    inline
    void logDiff(std::ostream& log, const char* text, bool lhs, bool rhs)
    {
	if (lhs != rhs)
	{
	    if (rhs)
		log << " -->" << text;
	    else
		log << " " << text << "-->";
	}
    }


    template<class Type>
    bool
    read_sysfs_property(const string& path, Type& value, bool log_error = true)
    {
	std::ifstream file(path);
	classic(file);
	file >> value;
	file.close();

	if (file.fail())
	{
	    if (log_error)
		y2err("reading " << path << " failed");
	    return false;
	}

	return true;
    }


    template<class Type>
    struct deref_less : public std::binary_function<const Type*, const Type*, bool>
    {
	bool operator()(const Type* x, const Type* y) const { return *x < *y; }
    };


    template<class Type>
    struct deref_equal_to : public std::binary_function<const Type*, const Type*, bool>
    {	
	bool operator()(const Type* x, const Type* y) const { return *x == *y; }
    };


    template <class Type>
    void pointerIntoSortedList(list<Type*>& l, Type* e)
    {
	l.insert(lower_bound(l.begin(), l.end(), e, deref_less<Type>()), e);
    }


    template <class Type>
    void clearPointerList(list<Type*>& l)
    {
	for (typename list<Type*>::iterator i = l.begin(); i != l.end(); ++i)
	    delete *i;
	l.clear();
    }


    template <typename ListType, typename Type>
    bool contains(const ListType& l, const Type& value)
    {
	return find(l.begin(), l.end(), value) != l.end();
    }


    template <typename ListType, typename Predicate>
    bool contains_if(const ListType& l, Predicate pred)
    {
	return find_if(l.begin(), l.end(), pred) != l.end();
    }


    template<typename Map, typename Key, typename Value>
    typename Map::iterator mapInsertOrReplace(Map& m, const Key& k, const Value& v)
    {
	typename Map::iterator pos = m.lower_bound(k);
	if (pos != m.end() && !typename Map::key_compare()(k, pos->first))
	    pos->second = v;
	else
	    pos = m.insert(pos, typename Map::value_type(k, v));
	return pos;
    }

    template<typename List, typename Value>
    typename List::const_iterator addIfNotThere(List& l, const Value& v)
    {
	typename List::const_iterator pos = find( l.begin(), l.end(), v );
	if (pos == l.end() )
	    pos = l.insert(l.end(), v);
	return pos;
    }


    template <class InputIterator1, class InputIterator2>
    bool equalContent(InputIterator1 first1, InputIterator1 last1,
		      InputIterator2 first2, InputIterator2 last2)
    {
	while (first1 != last1 && first2 != last2)
	{
	    if (!first1->equalContent(*first2))
		return false;
	    ++first1;
	    ++first2;
	}
	return first1 == last1 && first2 == last2;
    }


    template <class InputIterator1, class InputIterator2>
    void logVolumesDifference(std::ostream& log, InputIterator1 first1, InputIterator1 last1,
			      InputIterator2 first2, InputIterator2 last2)
    {
	for (InputIterator1 i = first1; i != last1; ++i)
	{
	    InputIterator2 j = first2;
	    while (j != last2 && (i->device() != j->device() || i->created() != j->created()))
		++j;
	    if (j != last2)
	    {
		if (!i->equalContent(*j))
		{
		    i->logDifference(log, *j);
		    log << std::endl;
		}
	    }
	    else
	    {
		log << "  -->" << *i << std::endl;
	    }
	}

	for (InputIterator2 i = first2; i != last2; ++i)
	{
	    InputIterator1 j = first1;
	    while (j != last1 && (i->device() != j->device() || i->created() != j->created()))
		++j;
	    if (j == last1)
	    {
		log << "  <--" << *i << std::endl;
	    }
	}
    }


    template <class T, unsigned int sz>
    inline unsigned int lengthof (T (&)[sz]) { return sz; }

}

#endif
