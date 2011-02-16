/*
 * Copyright (c) [2004-2010] Novell, Inc.
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


#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <string>
#include <boost/algorithm/string.hpp>

#include <blocxx/AppenderLogger.hpp>
#include <blocxx/FileAppender.hpp>
#include <blocxx/Logger.hpp>
#include <blocxx/LogMessage.hpp>

#include "storage/AsciiFile.h"
#include "storage/StorageTmpl.h"
#include "storage/AppUtil.h"
#include "storage/StorageTypes.h"


namespace storage
{
    using namespace std;


void createPath(const string& Path_Cv)
{
  string::size_type Pos_ii = 0;
  while ((Pos_ii = Path_Cv.find('/', Pos_ii + 1)) != string::npos)
    {
      string Tmp_Ci = Path_Cv.substr(0, Pos_ii);
      mkdir(Tmp_Ci.c_str(), 0777);
    }
  mkdir(Path_Cv.c_str(), 0777);
}


bool
checkDir(const string& Path_Cv)
{
  struct stat Stat_ri;

  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISDIR(Stat_ri.st_mode));
}


bool
getStatMode(const string& Path_Cv, mode_t& val )
{
  struct stat Stat_ri;
  int ret_ii = stat(Path_Cv.c_str(), &Stat_ri);

  if( ret_ii==0 )
    val = Stat_ri.st_mode;
  else
    y2mil( "stat " << Path_Cv << " ret:" << ret_ii );

  return (ret_ii==0);
}

bool
setStatMode(const string& Path_Cv, mode_t val )
{
  int ret_ii = chmod( Path_Cv.c_str(), val );
  if( ret_ii!=0 )
    y2mil( "chmod " << Path_Cv << " ret:" << ret_ii );
  return( ret_ii==0 );
}


bool
checkNormalFile(const string& Path_Cv)
{
  struct stat Stat_ri;

  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISREG(Stat_ri.st_mode));
}


    list<string>
    glob(const string& path, int flags)
    {
	list<string> ret;

	glob_t globbuf;
	if (glob(path.c_str(), flags, 0, &globbuf) == 0)
	{
	    for (char** p = globbuf.gl_pathv; *p != 0; p++)
		ret.push_back(*p);
	}
	globfree (&globbuf);

	return ret;
    }


    bool
    getStatVfs(const string& path, StatVfs& buf)
    {
	struct statvfs64 fsbuf;
	if (statvfs64(path.c_str(), &fsbuf) != 0)
	{
	    buf.sizeK = buf.freeK = 0;

	    y2err("errno:" << errno << " " << strerror(errno));
	    return false;
	}

	buf.sizeK = fsbuf.f_blocks;
	buf.sizeK *= fsbuf.f_bsize;
	buf.sizeK /= 1024;

	buf.freeK = fsbuf.f_bfree;
	buf.freeK *= fsbuf.f_bsize;
	buf.freeK /= 1024;

	y2mil("blocks:" << fsbuf.f_blocks << " bfree:" << fsbuf.f_bfree <<
	      " bsize:" << fsbuf.f_bsize << " sizeK:" << buf.sizeK <<
	      " freeK:" << buf.freeK);
	return true;
    }


    bool
    getMajorMinor(const string& device, unsigned long& major, unsigned long& minor)
    {
	bool ret = false;
	string dev = normalizeDevice(device);
	struct stat sbuf;
	if (stat(device.c_str(), &sbuf) == 0)
	{
	    major = gnu_dev_major(sbuf.st_rdev);
	    minor = gnu_dev_minor(sbuf.st_rdev);
	    ret = true;
	}
	else
	{
	    y2err("stat for " << device << " failed errno:" << errno << " (" << strerror(errno) << ")");
	}
	return ret;
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


    string normalizeDevice(const string& dev)
    {
	if (!boost::starts_with(dev, "/dev/") && !isNfsDev(dev))
	    return "/dev/" + dev;
	return dev;
    }


    list<string> normalizeDevices(const list<string>& devs)
    {
	list<string> ret;
	for (list<string>::const_iterator it = devs.begin(); it != devs.end(); ++it)
	    ret.push_back(normalizeDevice(*it));
	return ret;
    }


bool isNfsDev( const string& dev )
    {
    return( !dev.empty() && dev[0]!='/' &&
            dev.find( ':' )!=string::npos );
    }


    string undevDevice(const string& dev)
    {
	if (boost::starts_with(dev, "/dev/"))
	    return string(dev, 5);
	return dev;
    }


static const blocxx::String component = "libstorage";


void createLogger(const string& name, const string& logpath, const string& logfile)
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
prepareLogStream(ostringstream& stream)
{
    stream.imbue(std::locale::classic());
    stream.setf(std::ios::boolalpha);
    stream.setf(std::ios::showbase);
}


ostringstream*
logStreamOpen()
{
    std::ostringstream* stream = new ostringstream;
    prepareLogStream(*stream);
    return stream;
}


void
logStreamClose(LogLevel level, const char* file, unsigned line, const char* func,
	       ostringstream* stream)
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
	string tmp = stream->str();

	string::size_type pos1 = 0;

	while (true)
	{
	    string::size_type pos2 = tmp.find('\n', pos1);

	    if (pos2 != string::npos || pos1 != tmp.length())
		LogAppender::getCurrentLogAppender()->logMessage(LogMessage(component, category,
									    String(tmp.substr(pos1, pos2 - pos1)),
									    file, line, func));

	    if (pos2 == string::npos)
		break;

	    pos1 = pos2 + 1;
	}
    }

    delete stream;
}


    string
    udevAppendPart(const string& s, unsigned num)
    {
	return s + "-part" + decString(num);
    }


    string
    udevEncode(const string& s)
    {
	string r = s;

	string::size_type pos = 0;

	while (true)
	{
	    pos = r.find_first_of(" '\\/", pos);
	    if (pos == string::npos)
		break;

	    char tmp[16];
	    sprintf(tmp, "\\x%02x", r[pos]);
	    r.replace(pos, 1, tmp);

	    pos += 4;
	}

	return r;
    }


    string
    udevDecode(const string& s)
    {
	string r = s;

	string::size_type pos = 0;

	while (true)
	{
	    pos = r.find("\\x", pos);
	    if (pos == string::npos || pos > r.size() - 4)
		break;

	    unsigned int tmp;
	    if (sscanf(r.substr(pos + 2, 2).c_str(), "%x", &tmp) == 1)
		r.replace(pos, 4, 1, (char) tmp);

	    pos += 1;
	}

	return r;
    }


    bool
    mkdtemp(string& path)
    {
	char* tmp = strdup(path.c_str());
	if (!::mkdtemp(tmp))
	{
	    free(tmp);
	    return false;
	}

	path = tmp;
	free(tmp);
	return true;
    }


bool
readlink(const string& path, string& buf)
{
    char tmp[1024];
    int count = ::readlink(path.c_str(), tmp, sizeof(tmp));
    if (count >= 0)
	buf = string(tmp, count);
    return count != -1;
}


    bool
    readlinkat(int fd, const string& path, string& buf)
    {
        char tmp[1024];
        int count = ::readlinkat(fd, path.c_str(), tmp, sizeof(tmp));
        if (count >= 0)
            buf = string(tmp, count);
        return count != -1;
    }


    map<string, string>
    getUdevLinks(const char* path)
    {
	map<string, string> links;

	int fd = open(path, O_RDONLY);
	if (fd >= 0)
	{
	    DIR* dir;
	    if ((dir = fdopendir(fd)) != NULL)
	    {
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL)
		{
		    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
			continue;

		    string tmp;
		    if (!readlinkat(fd, entry->d_name, tmp))
			continue;

		    string::size_type pos = tmp.find_first_not_of("./");
		    if (pos != string::npos)
			links[udevDecode(entry->d_name)] = tmp.substr(pos);
		}
		closedir(dir);
	    }
	    else
	    {
		close(fd);
	    }
	}

	return links;
    }


    UdevMap::UdevMap(const string& path)
    {
	y2mil("path: " << path);

	const map<string, string> links = getUdevLinks(path.c_str());

	for (map<string, string>::const_iterator it = links.begin(); it != links.end(); ++it)
	    data[it->second].push_back(it->first);

	for (const_iterator it = begin(); it != end(); ++it)
	    y2mil("data[" << it->first << "] -> " << boost::join(it->second, " "));
    }


unsigned
getMajorDevices(const char* driver)
{
    unsigned ret = 0;

    AsciiFile file("/proc/devices");
    const vector<string>& lines = file.lines();

    Regex rx("^" + Regex::ws + "([0-9]+)" + Regex::ws + string(driver) + "$");

    if (find_if(lines, regex_matches(rx)) != lines.end())
	rx.cap(1) >> ret;
    else
	y2err("did not find " << driver << " in /proc/devices");

    y2mil("driver:" << driver << " ret:" << ret);
    return ret;
}


    void
    Text::clear()
    {
	native.clear();
	text.clear();
    }


    const Text&
    Text::operator+=(const Text& a)
    {
	native += a.native;
	text += a.text;
	return *this;
    }


    string
    sformat(const string& format, va_list ap)
    {
	char* result;

	if (vasprintf(&result, format.c_str(), ap) == -1)
	    return string();

	string str(result);
	free(result);
	return str;
    }


    Text
    sformat(const Text& format, ...)
    {
	Text text;
	va_list ap;

	va_start(ap, format);
	text.native = sformat(format.native, ap);
	va_end(ap);

	va_start(ap, format);
	text.text = sformat(format.text, ap);
	va_end(ap);

	return text;
    }


    Text _(const char* msgid)
    {
	return Text(msgid, dgettext("libstorage", msgid));
    }

    Text _(const char* msgid, const char* msgid_plural, unsigned long int n)
    {
	return Text(n == 1 ? msgid : msgid_plural, dngettext("libstorage", msgid, msgid_plural, n));
    }


    string
    hostname()
    {
	struct utsname buf;
	if (uname(&buf) != 0)
	    return string("unknown");
	string hostname(buf.nodename);
	if (strlen(buf.domainname) > 0)
	    hostname += "." + string(buf.domainname);
	return hostname;
    }


    string
    datetime()
    {
	time_t t1 = time(NULL);
	struct tm t2;
	gmtime_r(&t1, &t2);
	char buf[64 + 1];
	if (strftime(buf, sizeof(buf), "%F %T %Z", &t2) == 0)
	    return string("unknown");
	return string(buf);
    }


    StopWatch::StopWatch()
    {
	gettimeofday(&start_tv, NULL);
    }


    std::ostream& operator<<(std::ostream& s, const StopWatch& sw)
    {
	struct timeval stop_tv;
	gettimeofday(&stop_tv, NULL);

	struct timeval tv;
	timersub(&stop_tv, &sw.start_tv, &tv);

	return s << fixed << double(tv.tv_sec) + (double)(tv.tv_usec) / 1000000.0 << "s";
    }


const string app_ws = " \t\n";

}
