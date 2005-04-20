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
string mergeString( const list<string>& l, const string& del=" " );
map<string,string> makeMap( const list<string>& l, const string& delim = "=",
			    const string& removeSur = " \t\n" );
void removeLastIf(string& Text_Cr, char Char_cv);
string kbyteToHumanString( unsigned long long size );
string normalizeDevice( const string& dev );
void normalizeDevice( string& dev );
bool runningFromSystem();

void delay(int Microsec_iv);

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

extern bool system_cmd_testmode;
extern const string app_ws;

#endif
