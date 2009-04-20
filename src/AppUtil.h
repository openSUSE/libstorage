#ifndef APP_UTIL_H
#define APP_UTIL_H

#include <libintl.h>
#include <sstream>
#include <locale>
#include <string>
#include <list>
#include <map>


namespace storage
{
    using std::string;
    using std::list;
    using std::map;


void createPath(const string& Path_Cv);
bool checkNormalFile(const string& Path_Cv);
bool checkDir(const string& Path_Cv);

string extractNthWord(int Num_iv, const string& Line_Cv, bool GetRest_bi = false);
std::list<string> splitString( const string& s, const string& delChars=" \t\n",
                          bool multipleDelim=true, bool skipEmpty=true,
			  const string& quotes="" );
std::map<string,string> makeMap( const std::list<string>& l,
                                 const string& delim = "=",
				 const string& removeSur = " \t\n" );

    bool readlink(const string& path, string& buf);

    map<string, list<string>> getUdevMap(const char* path);
    map<string, string> getRevUdevMap(const char* path);

string normalizeDevice( const string& dev );
void normalizeDevice( string& dev );
string undevDevice( const string& dev );
void undevDevice( string& dev );
bool isNfsDev( const string& dev );

unsigned getMajorDevices(const char* driver);


template<class StreamType>
void classic(StreamType& stream)
{
    stream.imbue(std::locale::classic());
}


enum LogLevel { DEBUG, MILESTONE, WARNING, ERROR };

void createLogger(const string& name, const string& logpath, const string& logfile);

bool testLogLevel(LogLevel level);

std::ostringstream* logStreamOpen();

void logStreamClose(LogLevel level, const char* file, unsigned line,
		    const char* func, std::ostringstream*);

#define y2deb(op) y2log_op(storage::DEBUG, __FILE__, __LINE__, __FUNCTION__, op)
#define y2mil(op) y2log_op(storage::MILESTONE, __FILE__, __LINE__, __FUNCTION__, op)
#define y2war(op) y2log_op(storage::WARNING, __FILE__, __LINE__, __FUNCTION__, op)
#define y2err(op) y2log_op(storage::ERROR, __FILE__, __LINE__, __FUNCTION__, op)

#define y2log_op(level, file, line, func, op)				\
    do {								\
	if (storage::testLogLevel(level))				\
	{								\
	    std::ostringstream* __buf = storage::logStreamOpen();	\
	    *__buf << op;						\
	    storage::logStreamClose(level, file, line, func, __buf);	\
	}								\
    } while (0)


string sformat(const char* format, ...);

    string hostname();
    string datetime();


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
