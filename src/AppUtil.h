// -*- C++ -*-
// Maintainer: fehr@suse.de

#ifndef _AppUtil_h
#define _AppUtil_h

#include <ycp/y2log.h>

#include <time.h>
#include <libintl.h>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <string>
#include <list>

using std::string;

class AsciiFile;

bool searchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr);
bool searchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr,
		int& StartLine_ir);
void timeMark(const char*const Text_pcv, bool PrintDiff_bi = true);
void createPath(string Path_Cv);

string extractNthWord(int Num_iv, string Line_Cv, bool GetRest_bi = false);
list<string> splitString( const string& s, const string& delChars=" \t\n",
                          bool multipleDelim=true, bool skipEmpty=true );
map<string,string> makeMap( const list<string>& l, const string& delim = "=",
			    const string& removeSur = " \t\n" );
void removeLastIf(string& Text_Cr, char Char_cv);
bool runningFromSystem();

void delay(int Microsec_iv);

template<class Num> string decString(Num number)
    {
    std::ostringstream num_str;
    num_str << number;
    return( num_str.str() );
    }

template<class Num> string hexString(Num number)
    {
    std::ostringstream num_str;
    num_str << std::hex << number;
    return( num_str.str() );
    }

template<class Value> void operator>>( const string& d, Value& v)
    {
    std::istringstream Data( d );
    Data >> v;
    }

template<class Value> ostream& operator<<( ostream& s, const list<Value>& l )
    {
    s << "<";
    for( typename list<Value>::const_iterator i=l.begin(); i!=l.end(); i++ )
	{
	if( i!=l.begin() )
	    s << " ";
	s << *i;
	}
    s << ">";
    return( s );
    }

template<class F, class S> ostream& operator<<( ostream& s, const pair<F,S>& p )
    {
    s << "[" << p.first << ":" << p.second << "]";
    }

template<class Key, class Value> ostream& operator<<( ostream& s, const map<Key,Value>& m )
    {
    s << "<";
    for( typename map<Key,Value>::const_iterator i=m.begin(); i!=m.end(); i++ )
	{
	if( i!=m.begin() )
	    s << " ";
	s << i->first << ":" << i->second;
	}
    s << ">";
    return( s );
    }

inline string sformat( const char* txt, ... )
    {
    unsigned s = strlen(txt)+3072;
    char * mem = new char[s];
    if( mem != NULL )
	{
	va_list p;
	va_start( p, txt );
	vsnprintf( mem, s-1, txt, p );
	va_end( p );
	mem[s-1]=0;
	y2milestone( "txt:%s", mem );
	string s( mem );
	delete [] mem;
	return( s );
	}
    else
	return( "" );
    }

inline const char* _(const char* msgid)
    {
    return( dgettext("storage", msgid) );
    }

inline const char* _(const char* msgid1, const char* msgid2, unsigned long int n)
    {
    return( dngettext ("storage", msgid1, msgid2, n) );
    }

#endif
