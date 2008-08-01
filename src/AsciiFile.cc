// Maintainer: fehr@suse.de
/* 
  Textdomain    "storage"
*/

#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>

#include "y2storage/AppUtil.h"
#include "y2storage/Regex.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/AsciiFile.h"

using namespace std;
using namespace storage;

#define DBG(x)

AsciiFile::AsciiFile( bool CreateBackup_bv, const char* BackupExt_Cv ) :
	BackupCreated_b(!CreateBackup_bv),
	BackupExtension_C( BackupExt_Cv )
{
}

AsciiFile::AsciiFile( const char* Name_Cv, bool CreateBackup_bv,
		      const char* BackupExt_Cv ) :
	BackupCreated_b(!CreateBackup_bv),
	BackupExtension_C( BackupExt_Cv )
    {
    loadFile( Name_Cv );
    }

AsciiFile::AsciiFile( const string& Name_Cv, bool CreateBackup_bv,
		      const char* BackupExt_Cv ) :
	BackupCreated_b(!CreateBackup_bv),
	BackupExtension_C( BackupExt_Cv )
    {
    loadFile( Name_Cv.c_str() );
    }

AsciiFile::~AsciiFile()
    {
    }

bool AsciiFile::loadFile( const string& Name_Cv )
    {
    bool Ret_bi;

    y2milestone( "Loading File:\"%s\"", Name_Cv.c_str() );
    Lines_C.clear();
    Ret_bi = appendFile( Name_Cv, Lines_C );
    Name_C = Name_Cv;
    return( Ret_bi );
    }

const string& AsciiFile::fileName() const
    {
    return( Name_C );
    }

bool AsciiFile::appendFile( const string& Name_Cv )
    {
    DBG( App_pC->Dbg() << "Appending File:\"" << Name_Cv << "\"\n"; )
    return( appendFile( Name_Cv, Lines_C ) );
    }

bool AsciiFile::appendFile( AsciiFile& File_Cv )
{
    DBG( App_pC->Dbg() << "Appending AsciiFile Lines:" << File_Cv.numLines() << "\n"; )
    return( appendFile( File_Cv, Lines_C ) );
}

bool AsciiFile::appendFile( const string& Name_Cv, vector<string>& Lines_Cr )
{
    ifstream File_Ci( Name_Cv.c_str() );
    string Line_Ci;

    bool Ret_bi = File_Ci.good();
    File_Ci.unsetf(ifstream::skipws);
    getline( File_Ci, Line_Ci );
    while( File_Ci.good() )
      {
	Lines_Cr.push_back( Line_Ci );
	getline( File_Ci, Line_Ci );
      }
    return( Ret_bi );
}

bool AsciiFile::appendFile( AsciiFile& File_Cv, vector<string>& Lines_Cr )
{
    unsigned Idx_ii = 0;

    while( Idx_ii<File_Cv.numLines() )
      {
	Lines_Cr.push_back( File_Cv[Idx_ii] );
	Idx_ii++;
      }
    return( true );
}

bool AsciiFile::insertFile( const string& Name_Cv, unsigned int BeforeLine_iv )
{
    ifstream File_Ci( Name_Cv.c_str() );
    string Line_Ci;
    vector<string> New_Ci;

    bool Ret_bi = File_Ci.good();
    if( Ret_bi )
      {
	unsigned int Idx_ii=0;
	while( Idx_ii<BeforeLine_iv )
	  {
	    New_Ci.push_back( Lines_C[Idx_ii] );
	    Idx_ii++;
	    }
	Ret_bi = appendFile( Name_Cv, New_Ci );
	while( Idx_ii<Lines_C.size() )
	    {
	    New_Ci.push_back( Lines_C[Idx_ii] );
	    Idx_ii++;
	    }
	Lines_C = New_Ci;
	}
    return( Ret_bi );
    }

bool AsciiFile::insertFile( AsciiFile& File_Cv, unsigned int BeforeLine_iv )
    {
    string Line_Ci;
    vector<string> New_Ci;
    bool Ret_bi;

    DBG( App_pC->Dbg() << "Inserting AsciiFile Lines:" << File_Cv.numLines()
                       << " before:" << BeforeLine_iv << endl; )
    unsigned int Idx_ii=0;
    while( Idx_ii<BeforeLine_iv )
	{
	New_Ci.push_back( Lines_C[Idx_ii] );
	Idx_ii++;
	}
    Ret_bi = appendFile( File_Cv, New_Ci );
    while( Idx_ii<Lines_C.size() )
	{
	New_Ci.push_back( Lines_C[Idx_ii] );
	Idx_ii++;
	}
    Lines_C = New_Ci;
    return( Ret_bi );
    }

bool AsciiFile::updateFile()
    {
    struct stat Stat_ri;
    bool Status_b = stat( Name_C.c_str(), &Stat_ri )==0;

    if( !BackupCreated_b )
	{
	string BakName_Ci = Name_C + BackupExtension_C;
	if( access( Name_C.c_str(), R_OK ) == 0 )
	    {
	    string OldBakName_Ci = BakName_Ci  + ".o";
	    if( access( BakName_Ci.c_str(), R_OK ) == 0 &&
		access( OldBakName_Ci.c_str(), R_OK ) != 0 )
		{
		int r = link( BakName_Ci.c_str(), OldBakName_Ci.c_str() );
		if( r==0 )
		    unlink( BakName_Ci.c_str() );
		}
	    SystemCmd Cmd_Ci;
	    string Tmp_Ci = "cp -a ";
	    Tmp_Ci += Name_C;
	    Tmp_Ci += ' ';
	    Tmp_Ci += BakName_Ci;
	    Cmd_Ci.execute( Tmp_Ci );
	    }
	BackupCreated_b = true;
	}
    ofstream File_Ci( Name_C.c_str() );
    unsigned int Idx_ii = 0;

    DBG( App_pC->Dbg() << "Writing File:\"" << Name_C << "\"\n"; )
    while( File_Ci.good() && Idx_ii<Lines_C.size() )
	{
	File_Ci << Lines_C[Idx_ii] << std::endl;
	Idx_ii++;
	}
    if( Status_b )
	{
	chmod( Name_C.c_str(), Stat_ri.st_mode );
	}
    return( File_Ci.good() );
    }

bool AsciiFile::removeIfEmpty()
    {
    bool ret = Lines_C.empty();
    if( ret && access( Name_C.c_str(), W_OK )==0 )
	{
	unlink( Name_C.c_str() );
	y2mil( "deleting file " << Name_C );
	}
    return( ret );
    }

bool AsciiFile::saveToFile( const string& Name_Cv )
    {
    ofstream File_Ci( Name_Cv.c_str() );
    unsigned int Idx_ii = 0;

    DBG( App_pC->Dbg() << "SaveToFile File:\"" << Name_Cv << "\"\n"; )
    while( File_Ci.good() && Idx_ii < Lines_C.size() )
	{
	File_Ci << Lines_C[Idx_ii] << std::endl;
	Idx_ii++;
	}
    return( File_Ci.good() );
    }


void AsciiFile::append( const string& Line_Cv )
    {
    string::size_type Idx_ii;
    string Line_Ci = Line_Cv;

    DBG( App_pC->Dbg() << "Append Line:\"" << Line_Ci << "\"\n"; )
    removeLastIf( Line_Ci, '\n' );
    while( (Idx_ii=Line_Ci.find( '\n' ))!= string::npos )
	{
	Lines_C.push_back( Line_Ci.substr(0, Idx_ii ) );
	Line_Ci.erase( 0, Idx_ii+1 );
	}
    Lines_C.push_back( Line_Ci );
    }

void AsciiFile::append( const list<string>& lines )
    {
    for( list<string>::const_iterator i=lines.begin(); i!=lines.end(); ++i )
	append( *i );
    }

void AsciiFile::replace( unsigned int Start_iv, unsigned int Cnt_iv, 
                         const string& Lines_Cv )
    {
    remove( Start_iv, Cnt_iv );
    insert( Start_iv, Lines_Cv );
    }

void AsciiFile::replace( unsigned int Start_iv, unsigned int Cnt_iv, 
                         const list<string>& lines )
    {
    remove( Start_iv, Cnt_iv );
    for( list<string>::const_reverse_iterator i=lines.rbegin(); i!=lines.rend();
         ++i )
	insert( Start_iv, *i );
    }

void AsciiFile::remove( unsigned int Start_iv, unsigned int Cnt_iv )
    {
    DBG( App_pC->Dbg() << "Delete Line From:" << Start_iv
                       << " Length:" << Cnt_iv << "\n"; )
    DBG( App_pC->Dbg() << "Before Delete Lines:" << Lines_C.size() << std::endl; );
    Start_iv = max( 0u, Start_iv );
    if( Start_iv < Lines_C.size() )
	{
	Cnt_iv = min( Cnt_iv, (unsigned int)(Lines_C.size())-Start_iv );
	for( unsigned int I_ii=Start_iv; I_ii< Lines_C.size()-Cnt_iv; I_ii++ )
	    {
	    Lines_C[I_ii] = Lines_C[I_ii+Cnt_iv];
	    }
	for( unsigned int I_ii=0; I_ii<Cnt_iv; I_ii++ )
	    {
	    Lines_C.pop_back();
	    }
	}
    DBG( App_pC->Dbg() << "After Delete Lines:" << Lines_C.size() << std::endl; );
    }

void AsciiFile::insert( unsigned int Before_iv, const string& Line_Cv )
    {
    unsigned int Idx_ii = Lines_C.size();
    DBG( App_pC->Dbg() << "Insert Line Before:" << Before_iv
                       << " High:" << Lines_C.size()
                       << " Text:\"" << Line_Cv << "\"\n"; )
    if( Before_iv>=Idx_ii )
	{
	append( Line_Cv );
	}
    else
	{
	string Line_Ci = Line_Cv;
	unsigned int Cnt_ii;
	string::size_type Pos_ii;

	removeLastIf( Line_Ci, '\n' );
	Cnt_ii = 0;
	Pos_ii = 0;
	do
	  Cnt_ii++;
	while (Pos_ii = Line_Ci.find('\n', Pos_ii), Pos_ii != string::npos);
	for( unsigned int I_ii=0; I_ii<Cnt_ii; I_ii++ )
	    {
	    Lines_C.push_back( "" );
	    }
	while( Idx_ii>Before_iv )
	    {
	    Idx_ii--;
	    Lines_C[Idx_ii+Cnt_ii] = Lines_C[Idx_ii];
	    }
	while( (Pos_ii=Line_Ci.find( '\n' ))!= string::npos )
	    {
	    Lines_C[Idx_ii++] = Line_Ci.substr( Pos_ii );
	    Line_Ci.erase( 0, Pos_ii+1 );
	    }
	Lines_C[Idx_ii] = Line_Ci;
	}
    }

const string& AsciiFile::operator [] ( unsigned int Idx_iv ) const
    {
    assert( Idx_iv < Lines_C.size( ) );
    return( Lines_C[Idx_iv] );
    }

string& AsciiFile::operator [] ( unsigned int Idx_iv )
    {
    assert( Idx_iv < Lines_C.size( ) );
    return( Lines_C[Idx_iv] );
    }

int AsciiFile::find( unsigned Start_iv, Regex& Pat_Cv ) const
    {
    unsigned Idx_ii = Start_iv;
    int Ret_ii = -1;
    while( Ret_ii<0 && Idx_ii<Lines_C.size() )
	{
	if( Pat_Cv.match( Lines_C[Idx_ii] ))
	    {
	    Ret_ii = Idx_ii;
	    }
	else
	    {
	    Idx_ii++;
	    }
	}
    DBG( App_pC->Dbg() << "Found Idx:" << Ret_ii
		       << " Start:" << Start_iv << std::endl; )
    if( Ret_ii>=0 )
	{
	DBG( App_pC->Dbg() << "Line:\"" << Lines_C[Idx_ii] << "\"\n"; )
	}
    return( Ret_ii );
    }

int AsciiFile::find( unsigned int Start_iv, const string& Pat_Cv ) const
    {
    string::size_type Pos_ii;
    unsigned int Idx_ii = Start_iv;
    int Ret_ii = -1;
    string Pat_Ci = Pat_Cv;
    bool BeginOfLine_bi = Pat_Ci.length()>0 && Pat_Ci[0]=='^';

    if( BeginOfLine_bi )
	{
	Pat_Ci.erase( 0, 1 );
	}
    while( Ret_ii<0 && Idx_ii<Lines_C.size() )
	{
	if( (Pos_ii=Lines_C[Idx_ii].find( Pat_Ci )) != string::npos )
	    {
	    if( !BeginOfLine_bi || (BeginOfLine_bi && Pos_ii==0) )
		{
		Ret_ii = Idx_ii;
		}
	    else
		{
		Idx_ii++;
		}
	    }
	else
	    {
	    Idx_ii++;
	    }
	}
    DBG( App_pC->Dbg() << "Found Idx:" << Ret_ii
		       << " Start:" << Start_iv << std::endl; )
    if( Ret_ii>=0 )
	{
	DBG( App_pC->Dbg() << "Line:\"" << Lines_C[Idx_ii] << "\"\n"; )
	}
    return( Ret_ii );
    }

unsigned AsciiFile::numLines() const
    {
    return( Lines_C.size() );
    }

unsigned AsciiFile::differentLine( const AsciiFile& File_Cv ) const
    {
    int Ret_ii = -1;
    unsigned Cnt_ii = min( numLines(), File_Cv.numLines() );
    unsigned I_ii = 0;
    while( I_ii<Cnt_ii && (*this)[I_ii]==File_Cv[I_ii] )
        {
        I_ii++;
        }
    if( I_ii<Cnt_ii )
        {
        Ret_ii = I_ii;
        }
    else if( numLines()>Cnt_ii || File_Cv.numLines()>Cnt_ii )
        {
        Ret_ii = Cnt_ii;
        }
    return( Ret_ii );
    }


void AsciiFile::removeLastIf (string& Text_Cr, char Char_cv) const
{
    if (Text_Cr.length() > 0 && Text_Cr[Text_Cr.length() - 1] == Char_cv)
	Text_Cr.erase(Text_Cr.length() - 1);
}


#if 0
void AsciiFile::put( const Regex& Pat_Cv, const String& Line_Cv )
    {
    int Idx_ii;

    if( (Idx_ii=Find( 0, Pat_Cv ))>=0 )
	{
	(*this)[Idx_ii] = Line_Cv;
	}
    else
	{
	append( Line_Cv );
	}
    }

#endif
