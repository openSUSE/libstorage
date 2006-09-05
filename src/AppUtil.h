// -*- C++ -*-
// Maintainer: fehr@suse.de

#ifndef _AppUtil_h
#define _AppUtil_h

#include <time.h>
#include <libintl.h>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <list>
#include <map>

using std::string;

namespace storage
{

class AsciiFile;

bool searchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr);
bool searchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr,
		int& StartLine_ir);
void tolower( string& s );
void timeMark(const char*const Text_pcv, bool PrintDiff_bi = true);
void createPath(string Path_Cv);
bool checkNormalFile(string Path_Cv);
bool checkDir(string Path_Cv);

string dupDash(const string& s);
string extractNthWord(int Num_iv, string Line_Cv, bool GetRest_bi = false);
std::list<string> splitString( const string& s, const string& delChars=" \t\n",
                          bool multipleDelim=true, bool skipEmpty=true,
			  const string& quotes="" );
string mergeString( const std::list<string>& l, const string& del=" " );
std::map<string,string> makeMap( const std::list<string>& l, 
                                 const string& delim = "=",
				 const string& removeSur = " \t\n" );
void getFindMap( const char* path, std::map<string,string>& m,
		 bool unique=true );
void getFindRevMap( const char* path, std::map<string,string>& m );
void removeLastIf(string& Text_Cr, char Char_cv);
string kbyteToHumanString( unsigned long long size );
string normalizeDevice( const string& dev );
void normalizeDevice( string& dev );
string undevDevice( const string& dev );
void undevDevice( string& dev );
bool runningFromSystem();
void delay(int Microsec_iv);
unsigned getMajorDevices( const string& driver );

int createLogger( const string& component, const string& name,
                  const string& logpath, const string& logfile );

void log_msg( unsigned level, const char* file, unsigned line, 
              const char* func, const char* add_str, const char* format, ... ) 
	__attribute__ ((format(printf, 6, 7)));
void log_msg( unsigned level, const char* file, unsigned line, 
              const char* func, const char* add_str, const char* format, ... ) 
	__attribute__ ((format(printf, 6, 7)));

#define y2debug(format, ...)  \
    log_msg( 0, __FILE__, __LINE__, __FUNCTION__, NULL, format, ##__VA_ARGS__ )
#define y2milestone(format, ...)  \
    log_msg( 1, __FILE__, __LINE__, __FUNCTION__, NULL, format, ##__VA_ARGS__ )
#define y2warning(format, ...)  \
    log_msg( 1, __FILE__, __LINE__, __FUNCTION__, "[WARNING]", format, ##__VA_ARGS__ )
#define y2error(format, ...)  \
    log_msg( 2, __FILE__, __LINE__, __FUNCTION__, NULL, format, ##__VA_ARGS__ )

#define y2deb(op) log_op( 0, __FILE__, __LINE__, __FUNCTION__, NULL, op )
#define y2mil(op) log_op( 1, __FILE__, __LINE__, __FUNCTION__, NULL, op )
#define y2war(op) log_op( 1, __FILE__, __LINE__, __FUNCTION__, "[WARNING]", op )
#define y2err(op) log_op( 2, __FILE__, __LINE__, __FUNCTION__, NULL, op )

#define log_op( level, file, line, function, add, op )                \
    {                                                                 \
    std::ostringstream __buf;                                         \
    __buf << op;                                                      \
    log_msg( level, file, line, function, add, __buf.str().c_str() ); \
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

#define IPC_PROJ_ID 7890

extern bool system_cmd_testmode;
extern const string app_ws;

}

#endif
