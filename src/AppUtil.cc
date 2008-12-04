// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/


#include <unistd.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <locale>
#include <boost/algorithm/string.hpp>

#include <blocxx/AppenderLogger.hpp>
#include <blocxx/FileAppender.hpp>
#include <blocxx/Logger.hpp>
#include <blocxx/LogMessage.hpp>

#include "y2storage/AsciiFile.h"
#include "y2storage/StorageTmpl.h"
#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"

using namespace std;

namespace storage
{

string dupDash(const string& s)
    {
    string ret(s);
    string::size_type pos = ret.find("-");
    while(pos!=string::npos)
	{
	ret.insert(pos,1,'-');
	pos = ret.find("-",pos+2);
	}
    return(ret);
    }

bool
searchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr)
{
  int LineNr_ii = 0;
  return searchFile(File_Cr, Pat_Cv, Line_Cr, LineNr_ii);
}

bool
searchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr, int& LineNr_ir)
{
  int End_ii;
  bool Found_bi = false;
  bool BeginOfLine_bi;
  string Tmp_Ci;
  int LineNr_ii;
  string Search_Ci(Pat_Cv);

  BeginOfLine_bi = Search_Ci.length() > 0 && Search_Ci[0] == '^';
  if (BeginOfLine_bi)
    Search_Ci.erase(0, 1);
  End_ii = File_Cr.numLines();
  LineNr_ii = LineNr_ir;
  while (!Found_bi && LineNr_ii < End_ii)
    {
      string::size_type Idx_ii;

      Tmp_Ci = File_Cr[LineNr_ii++];
      Idx_ii = Tmp_Ci.find(Search_Ci);
      if (Idx_ii != string::npos)
	{
	  if (BeginOfLine_bi)
	    Found_bi = Idx_ii == 0;
	  else
	    Found_bi = true;
	}
    }
  if (Found_bi)
    {
      Line_Cr = Tmp_Ci;
      LineNr_ir = LineNr_ii - 1;
    }
  return Found_bi;
}


void createPath(string Path_Cv)
{
  string Path_Ci = Path_Cv;
  string Tmp_Ci;

  string::size_type Pos_ii = 0;
  while ((Pos_ii = Path_Ci.find('/', Pos_ii + 1)) != string::npos)
    {
      Tmp_Ci = Path_Ci.substr(0, Pos_ii);
      mkdir(Tmp_Ci.c_str(), 0777);
    }
  mkdir(Path_Ci.c_str(), 0777);
}

bool
checkDir(string Path_Cv)
{
  struct stat Stat_ri;

  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISDIR(Stat_ri.st_mode));
}

bool
checkSymlink(string Path_Cv)
{
  struct stat Stat_ri;

  return (lstat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISLNK(Stat_ri.st_mode));
}

bool
checkBlockDevice(string Path_Cv)
{
  struct stat Stat_ri;

  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISBLK(Stat_ri.st_mode));
}

bool
checkNormalFile(string Path_Cv)
{
  struct stat Stat_ri;

  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISREG(Stat_ri.st_mode));
}

string extractNthWord(int Num_iv, const string& Line_Cv, bool GetRest_bi)
  {
  string::size_type pos;
  int I_ii=0;
  string Ret_Ci = Line_Cv;

  if( Ret_Ci.find_first_of(app_ws)==0 )
    {
    pos = Ret_Ci.find_first_not_of(app_ws);
    if( pos != string::npos )
        {
        Ret_Ci.erase(0, pos );
        }
    else
        {
        Ret_Ci.erase();
        }
    }
  while( I_ii<Num_iv && Ret_Ci.length()>0 )
    {
    pos = Ret_Ci.find_first_of(app_ws);
    if( pos != string::npos )
        {
        Ret_Ci.erase(0, pos );
        }
    else
        {
        Ret_Ci.erase();
        }
    if( Ret_Ci.find_first_of(app_ws)==0 )
        {
        pos = Ret_Ci.find_first_not_of(app_ws);
        if( pos != string::npos )
            {
            Ret_Ci.erase(0, pos );
            }
        else
            {
            Ret_Ci.erase();
            }
        }
    I_ii++;
    }
  if (!GetRest_bi && (pos=Ret_Ci.find_first_of(app_ws))!=string::npos )
      Ret_Ci.erase(pos);
  return Ret_Ci;
  }

list<string> splitString( const string& s, const string& delChars,
		          bool multipleDelim, bool skipEmpty,
			  const string& quotes )
    {
    string::size_type pos;
    string::size_type cur = 0;
    string::size_type nfind = 0;
    list<string> ret;

    while( cur<s.size() && (pos=s.find_first_of(delChars,nfind))!=string::npos )
	{
	if( pos==cur )
	    {
	    if( !skipEmpty )
		ret.push_back( "" );
	    }
	else
	    ret.push_back( s.substr( cur, pos-cur ));
	if( multipleDelim )
	    {
	    cur = s.find_first_not_of(delChars,pos);
	    }
	else
	    cur = pos+1;
	nfind = cur;
	if( !quotes.empty() )
	    {
	    string::size_type qpos=s.find_first_of(quotes,cur);
	    string::size_type lpos=s.find_first_of(delChars,cur);
	    if( qpos!=string::npos && qpos<lpos &&
	        (qpos=s.find_first_of(quotes,qpos+1))!=string::npos )
		{
		nfind = qpos;
		}
	    }
	}
    if( cur<s.size() )
	ret.push_back( s.substr( cur ));
    if( !skipEmpty && !s.empty() && s.find_last_of(delChars)==s.size()-1 )
	ret.push_back( "" );
    //y2mil( "ret:" << ret );
    return( ret );
    }


string
mergeString(const list<string>& l, const string& del)
{
    return boost::join(l, del);
}


map<string,string>
makeMap( const list<string>& l, const string& delim, const string& removeSur )
    {
    map<string,string> ret;
    for( list<string>::const_iterator i=l.begin(); i!=l.end(); ++i )
	{
	string k, v;
	string::size_type pos;
	if( (pos=i->find_first_of( delim ))!=string::npos )
	    {
	    k = i->substr( 0, pos );
	    string::size_type pos2 = i->find_first_not_of( delim, pos+1 );
	    if( pos2 != string::npos )
		v = i->substr( pos2 );
	    }
	if( !removeSur.empty() )
	    {
	    if( (pos=k.find_first_of(removeSur)) != string::npos )
		k.erase( 0, k.find_first_not_of(removeSur) );
	    if( !k.empty() && (pos=k.find_last_of(removeSur))==k.size()-1 )
		k.erase( k.find_last_not_of(removeSur)+1 );
	    if( (pos=v.find_first_of(removeSur)) != string::npos )
		v.erase( 0, v.find_first_not_of(removeSur) );
	    if( !v.empty() && (pos=v.find_last_of(removeSur))==v.size()-1 )
		v.erase( v.find_last_not_of(removeSur)+1 );
	    }
	if( !k.empty() && !v.empty() )
	    ret[k] = v;
	}
    return( ret );
    }


string normalizeDevice( const string& dev )
    {
    string ret( dev );
    normalizeDevice( ret );
    return( ret );
    }

bool isNfsDev( const string& dev )
    {
    return( !dev.empty() && dev[0]!='/' &&
            dev.find( ':' )!=string::npos );
    }

void normalizeDevice( string& dev )
    {
    if( dev.find( "/dev/" )!=0 && !isNfsDev(dev) )
	dev = "/dev/" + dev;
    }

string undevDevice( const string& dev )
    {
    string ret( dev );
    undevDevice( ret );
    return( ret );
    }

void undevDevice( string& dev )
    {
    if( dev.find( "/dev/" )==0 )
	dev.erase( 0, 5 );
    }


static const blocxx::String component = "libstorage";


void createLogger(const string& lcomponent, const string& name,
		  const string& logpath, const string& logfile)
{
    using namespace blocxx;

    if (logpath != "NULL" && logfile != "NULL")
    {
	String nm = name.c_str();
	LoggerConfigMap configItems;
	LogAppenderRef logApp;
	if (logpath != "STDERR" && logfile != "STDERR" &&
	    logpath != "SYSLOG" && logfile != "SYSLOG")
	{
	    String StrKey;
	    String StrPath;
	    StrKey.format("log.%s.location", name.c_str());
	    StrPath = (logpath + "/" + logfile).c_str();
	    configItems[StrKey] = StrPath;
	    logApp =
		LogAppender::createLogAppender(nm, LogAppender::ALL_COMPONENTS,
					       LogAppender::ALL_CATEGORIES,
					       "%d %-5p %c(%P) %F(%M):%L - %m",
					       LogAppender::TYPE_FILE,
					       configItems);
	}
	else if (logpath == "STDERR" && logfile == "STDERR")
	{
	    logApp =
		LogAppender::createLogAppender(nm, LogAppender::ALL_COMPONENTS,
					       LogAppender::ALL_CATEGORIES,
					       "%d %-5p %c(%P) %F(%M):%L - %m",
					       LogAppender::TYPE_STDERR,
					       configItems);
	}
	else
	{
	    logApp =
		LogAppender::createLogAppender(nm, LogAppender::ALL_COMPONENTS,
					       LogAppender::ALL_CATEGORIES,
					       "%d %-5p %c(%P) %F(%M):%L - %m",
					       LogAppender::TYPE_SYSLOG,
					       configItems);
	}

	LogAppender::setDefaultLogAppender(logApp);
    }
}


bool
testLogLevel(LogLevel level)
{
    using namespace blocxx;

    ELogLevel curLevel = LogAppender::getCurrentLogAppender()->getLogLevel();

    switch (level)
    {
	case DEBUG:
	    return curLevel >= ::blocxx::E_DEBUG_LEVEL;
	case MILESTONE:
	    return curLevel >= ::blocxx::E_INFO_LEVEL;
	case WARNING:
	    return curLevel >= ::blocxx::E_WARNING_LEVEL;
	case ERROR:
	    return curLevel >= ::blocxx::E_ERROR_LEVEL;
	default:
	    return curLevel >= ::blocxx::E_FATAL_ERROR_LEVEL;
    }
}


void
prepareLogStream(std::ostringstream& s)
{
    s.imbue(std::locale::classic());
    s.setf(std::ios::showbase);
}


void
logMsg(LogLevel level, const char* file, unsigned line, const char* func,
       const string& str)
{
    using namespace blocxx;

    ELogLevel curLevel = LogAppender::getCurrentLogAppender()->getLogLevel();
    String category;

    switch (level)
    {
	case DEBUG:
	    if (curLevel >= ::blocxx::E_DEBUG_LEVEL)
	    	category = Logger::STR_DEBUG_CATEGORY;
	    break;
	case MILESTONE:
	    if (curLevel >= ::blocxx::E_INFO_LEVEL)
	    	category = Logger::STR_INFO_CATEGORY;
	    break;
	case WARNING:
	    if (curLevel >= ::blocxx::E_WARNING_LEVEL)
		category = Logger::STR_WARNING_CATEGORY;
	    break;
	case ERROR:
	    if (curLevel >= ::blocxx::E_ERROR_LEVEL)
		category = Logger::STR_ERROR_CATEGORY;
	    break;
	default:
	    if (curLevel >= ::blocxx::E_FATAL_ERROR_LEVEL)
		category = Logger::STR_FATAL_CATEGORY;
	    break;
    }

    if (!category.empty())
    {
	LogAppender::getCurrentLogAppender()->logMessage(LogMessage(component, category,
								    String(str), file,
								    line, func));
    }
}


void
logMsgVaArgs(LogLevel level, const char* file, unsigned line, const char* func,
	     const char* format, ...)
{
    if (testLogLevel(level))
    {
	char* str;
	va_list ap;

	va_start(ap, format);
	if (vasprintf(&str, format, ap) == -1)
	    return;
	va_end(ap);

	logMsg(level, file, line, func, str);

	free(str);
    }
}


int
readlink(const char* path, string& buf)
{
    const int size = 1024;
    char tmp[size];
    int ret = ::readlink(path, tmp, size);
    if (ret >= 0)
	buf = string(tmp, ret);
    return ret;
}


map<string, string>
getUdevLinks(const char* path)
{
    map<string, string> links;

    DIR* dir;
    if ((dir = opendir(path)) != NULL)
    {
	struct dirent* entry;
	while ((entry = readdir(dir)) != NULL)
	{
	    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		continue;

	    string full = string(path) + "/" + entry->d_name;

	    string tmp;
	    if (readlink(full.c_str(), tmp) != -1)
	    {
		string::size_type pos = tmp.find_last_of('/');
		if (pos != string::npos)
		    links[entry->d_name] = tmp.substr(pos + 1);
		else
		    y2war("ignoring strange udev link \"" << entry->d_name <<
			  "\" -> \"" << tmp << "\" in \"" << path << "\"");
	    }
	}
	closedir(dir);
    }

    return links;
}


void
getUdevMap(const char* path, map<string, list<string>>& m)
{
    y2mil( "path: " << path );

    map<string, string> links = getUdevLinks(path);

    m.clear();
    for (map<string, string>::const_iterator it = links.begin(); it != links.end(); it++)
	m[it->second].push_back(it->first);

    y2mil("map: " << m);
}


void
getRevUdevMap(const char* path, map<string, string>& m)
{
    y2mil("path: " << path);

    m = getUdevLinks(path);

    y2mil("map: " << m);
}


unsigned getMajorDevices( const string& driver )
    {
    unsigned ret=0;
    string cmd = "grep \" " + driver + "$\" /proc/devices";
    SystemCmd c( cmd );
    if( c.numLines()>0 )
	{
	extractNthWord( 0, *c.getLine(0)) >> ret;
	}
    y2mil( "driver:" << driver << " ret:" << ret );
    return( ret );
    }


string escapeHtml(const string& str)
{
    string ret = str;  
    boost::replace_all(ret, "&", "&amp;");
    boost::replace_all(ret, "<", "&lt;");
    boost::replace_all(ret, ">", "&gt;");
    boost::replace_all(ret, "\"", "&quot;");
    boost::replace_all(ret, "'", "&apos;");
    return ret;
}


string sformat(const char* format, ...)
{
    char* result;
    va_list ap;

    va_start(ap, format);
    if (vasprintf(&result, format, ap) == -1)
	return string();
    va_end(ap);

    string str(result);
    free(result);
    return str;
}


int
numSuffixes()
{
    return 6;
}


string
getSuffix(int i, bool classic, bool sloppy = false)
{
    switch (i)
    {
	case 0:
	    if (sloppy)
		return "";
	    else
		/* Byte abbreviated */
		return classic ? "B" : _("B");

	case 1:
	    if (sloppy)
		/* Kilo abbreviated */
		return classic ? "k" : _("k");
	    else
		/* KiloByte abbreviated */
		return classic ? "kB" : _("kB");

	case 2:
	    if (sloppy)
		/* Mega abbreviated */
		return classic ? "M" : _("M");
	    else
		/* MegaByte abbreviated */
		return classic ? "MB" : _("MB");

	case 3:
	    if (sloppy)
		/* Giga abbreviated */
		return classic ? "G" : _("G");
	    else
		/* GigaByte abbreviated */
		return classic ? "GB" : _("GB");

	case 4:
	    if (sloppy)
		/* Tera abbreviated */
		return classic ? "T" : _("T");
	    else
		/* TeraByte abbreviated */
		return classic ? "TB" : _("TB");

	case 5:
	    if (sloppy)
		/* Peta abbreviated */
		return classic ? "P" : _("P");
	    else
		/* PetaByte abbreviated */
		return classic ? "PB" : _("PB");
    }

    return string("error");
}


string
byteToHumanString(unsigned long long size, bool classic, int precision, bool omit_zeroes)
{
    const locale loc = classic ? locale::classic() : locale();

    double f = size;
    int i = 0;

    while (f >= 1024.0 && i + 1 < numSuffixes())
    {
	f /= 1024.0;
	i++;
    }

    if (omit_zeroes && (f == (unsigned long long)(f)))
    {
	precision = 0;
    }

    ostringstream s;
    s.imbue(loc);
    s.setf(ios::fixed);
    s.precision(precision);

    s << f << ' ' << getSuffix(i, classic);

    return s.str();
}


bool
humanStringToByte(const string& str, bool classic, unsigned long long& size)
{
    const locale loc = classic ? locale::classic() : locale();

    const string str_trimmed = boost::trim_copy(str, loc);

    double f = 1.0;

    for (int i = 0; i < numSuffixes(); i++)
    {
	for (int j = 0; j < (classic ? 1 : 2); j++)
	{
	    string suffix = getSuffix(i, classic, j != 0);
	    if (boost::iends_with(str_trimmed, suffix, loc))
	    {
		string number = str_trimmed.substr(0, str_trimmed.size() - suffix.size());

		istringstream s(boost::trim_copy(number, loc));
		s.imbue(loc);

		double g;
		s >> g;

		if (!s.fail() && s.eof() && g >= 0.0)
		{
		    size = g * f;
		    return true;
		}
	    }
	}

	f *= 1024.0;
    }

    return false;
}


const string app_ws = " \t\n";

}
