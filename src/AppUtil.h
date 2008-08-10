#ifndef APP_UTIL_H
#define APP_UTIL_H

#include <time.h>
#include <libintl.h>
#include <string.h>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <locale>
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
string normalizeDevice( const string& dev );
void normalizeDevice( string& dev );
string undevDevice( const string& dev );
void undevDevice( string& dev );
bool isNfsDev( const string& dev );
unsigned getMajorDevices( const string& driver );


template<class StreamType>
void classic(StreamType& stream)
{
    stream.imbue(std::locale::classic());
}


void createLogger(const string& component, const string& name,
		  const string& logpath, const string& logfile);

void logMsg(unsigned level, const char* file, unsigned line,
	    const char* func, const string& str);

void logMsgVaArgs(unsigned level, const char* file, unsigned line,
		  const char* func, const char* format, ...)
    __attribute__ ((format(printf, 5, 6)));

#define y2debug(format, ...) \
    logMsgVaArgs(0, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define y2milestone(format, ...) \
    logMsgVaArgs(1, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define y2warning(format, ...) \
    logMsgVaArgs(2, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)
#define y2error(format, ...) \
    logMsgVaArgs(3, __FILE__, __LINE__, __FUNCTION__, format, ##__VA_ARGS__)

#define y2deb(op) y2log_op(0, __FILE__, __LINE__, __FUNCTION__, op)
#define y2mil(op) y2log_op(1, __FILE__, __LINE__, __FUNCTION__, op)
#define y2war(op) y2log_op(2, __FILE__, __LINE__, __FUNCTION__, op)
#define y2err(op) y2log_op(3, __FILE__, __LINE__, __FUNCTION__, op)

#define y2log_op(level, file, line, function, op)			\
    do {								\
	std::ostringstream __buf;					\
	classic(__buf);							\
	__buf << op;							\
	logMsg(level, file, line, function, __buf.str());		\
    } while (0)


string sformat(const char* format, ...);


string byteToHumanString(unsigned long long size, bool classic, int precision,
			 bool omit_zeroes);

bool humanStringToByte(const string& str, bool classic, unsigned long long& size);


inline const char* _(const char* msgid)
{
    return dgettext("storage", msgid);
}

inline const char* _(const char* msgid, const char* msgid_plural, unsigned long int n)
{
    return dngettext("storage", msgid, msgid_plural, n);
}


extern const string app_ws;

}

#endif
