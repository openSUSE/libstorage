// Maintainer: fehr@suse.de

#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <unistd.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/time.h>
#include <ycp/y2log.h>

#include <iomanip>
#include <string>

#include "y2storage/AsciiFile.h"
#include "y2storage/AppUtil.h"


// #define MEMORY_DEBUG

#define BUF_SIZE 512

bool
SearchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr)
{
  int LineNr_ii = 0;
  return SearchFile(File_Cr, Pat_Cv, Line_Cr, LineNr_ii);
}

bool
SearchFile(AsciiFile& File_Cr, string Pat_Cv, string& Line_Cr, int& LineNr_ir)
{
  int End_ii;
  bool Found_bi = false;
  bool BeginOfLine_bi;
  string Tmp_Ci;
  int LineNr_ii;
  string Search_Ci(Pat_Cv);

#ifdef MEMORY_DEBUG
  //  MemoryTrace();
  mcheck(MemoryAbort);
#endif
  BeginOfLine_bi = Search_Ci.length() > 0 && Search_Ci[0] == '^';
  if (BeginOfLine_bi)
    Search_Ci.erase(0, 1);
  End_ii = File_Cr.NumLines();
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

void TimeMark(const char*const Text_pcv, bool PrintDiff_bi)
{
  static unsigned long Start_ls;
  unsigned long Diff_li;
  struct timeb Time_ri;

  if (PrintDiff_bi)
    {
      ftime(&Time_ri);
      Diff_li = Time_ri.time % 1000000 * 1000 + Time_ri.millitm - Start_ls;
    }
  else
    {
      ftime(&Time_ri);
      Start_ls = Time_ri.time % 1000000 * 1000 + Time_ri.millitm;
    }
}

void CreatePath(string Path_Cv)
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
GetUid(string Name_Cv, unsigned& Uid_ir)
{
  struct passwd *Passwd_pri;

  Passwd_pri = getpwnam(Name_Cv.c_str());
  Uid_ir = Passwd_pri ? Passwd_pri->pw_uid : 0;
  return Passwd_pri != NULL;
}

bool
GetGid(string Name_Cv, unsigned& Gid_ir)
{
  struct group *Group_pri;

  Group_pri = getgrnam(Name_Cv.c_str());
  Gid_ir = Group_pri ? Group_pri->gr_gid : 0;
  return Group_pri != NULL;
}

int
GetGid(string Name_Cv)
{
  unsigned Tmp_ii;

  GetGid(Name_Cv, Tmp_ii);
  return Tmp_ii;
}

int
GetUid(string Name_Cv)
{
  unsigned Tmp_ii;

  GetUid(Name_Cv, Tmp_ii);
  return Tmp_ii;
}

bool
CheckDir(string Path_Cv)
{
  struct stat Stat_ri;

  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISDIR(Stat_ri.st_mode));
}

bool
CheckSymlink(string Path_Cv)
{
  struct stat Stat_ri;

  return (lstat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISLNK(Stat_ri.st_mode));
}

bool
CheckBlockDevice(string Path_Cv)
{
  struct stat Stat_ri;

  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISBLK(Stat_ri.st_mode));
}

bool
CheckNormalFile(string Path_Cv)
{
  struct stat Stat_ri;
  
  return (stat(Path_Cv.c_str(), &Stat_ri) >= 0 &&
	  S_ISREG(Stat_ri.st_mode));
}

string ExtractNthWord(int Num_iv, string Line_Cv, bool GetRest_bi)
  {
  string::size_type pos;
  int I_ii=0;
  string Ret_Ci = Line_Cv;

  if( Ret_Ci.find_first_of(" \t\n")==0 )
    {
    pos = Ret_Ci.find_first_not_of(" \t\n");
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
    pos = Ret_Ci.find_first_of(" \t\n");
    if( pos != string::npos )
        {
        Ret_Ci.erase(0, pos );
        }
    else
        {
        Ret_Ci.erase();
        }
    if( Ret_Ci.find_first_of(" \t\n")==0 )
        {
        pos = Ret_Ci.find_first_not_of(" \t\n");
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
  if (!GetRest_bi && (pos=Ret_Ci.find_first_of(" \t\n"))!=string::npos )
      Ret_Ci.erase(pos);
  return Ret_Ci;
  }

void PutNthWord(int Num_iv, string Word_Cv, string& Line_Cr)
{
  string Last_Ci = ExtractNthWord(Num_iv, Line_Cr, true);
  int Len_ii = Last_Ci.find_first_of(" \t");
  Line_Cr.replace(Line_Cr.length() - Last_Ci.length(), Len_ii, Word_Cv);
}


void RemoveLastIf (string& Text_Cr, char Char_cv)
{
  if (Text_Cr.length() > 0 && Text_Cr[Text_Cr.length() - 1] == Char_cv)
    Text_Cr.erase(Text_Cr.length() - 1);
}

void Delay(int Microsec_iv)
{
  timeval Timeout_ri;

  Timeout_ri.tv_sec = Microsec_iv / 1000000;
  Timeout_ri.tv_usec = Microsec_iv % 1000000;
  select(0, NULL, NULL, NULL, &Timeout_ri);
}

void RemoveSurrounding(char Delim_ci, string& Text_Cr)
{
  if (Text_Cr.length() > 0 && Text_Cr[0] == Delim_ci)
    Text_Cr.erase(0, 1);
  if (Text_Cr.length() > 0 && Text_Cr[Text_Cr.length() - 1] == Delim_ci)
    Text_Cr.erase(Text_Cr.length() - 1);
}

int
CompareGt(string Lhs_Cv, string Rhs_Cv)
{
  return Lhs_Cv > Rhs_Cv;
}

bool RunningFromSystem()
    {
    static bool FirstCall_bs = true;
    static bool FromSystem_bs;
    if( FirstCall_bs )
        {
        FirstCall_bs = false;
	FromSystem_bs = access( "/usr/lib/YaST2/.Reh", R_OK )!=0;
	y2milestone( "RunningFromSystem %d", FromSystem_bs );
        }
    return( FromSystem_bs );
    }


