// Maintainer: fehr@suse.de

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/vfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <ycp/y2log.h>

#include <string>

#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"

int SystemCmd::Nr_i = 0;

//#define FULL_DEBUG_SYSTEM_CMD

SystemCmd::SystemCmd( const string& Command_Cv, bool UseTmp_bv ) :
	Combine_b(false),
	UseTmp_b(UseTmp_bv),
	OutputHandler_f(NULL),
	HandlerPar_p(NULL)
    {
    y2milestone( "Konstruktor SystemCmd:\"%s\" UseTmp:%d Nr:%d", 
                 Command_Cv.c_str(), UseTmp_bv, Nr_i );
    Append_ab[0] = false;
    Append_ab[1] = false;
    initFile();
    execute( Command_Cv );
    }

SystemCmd::SystemCmd( bool UseTmp_bv ) :
	Combine_b(false),
	UseTmp_b(UseTmp_bv),
	OutputHandler_f(NULL),
	HandlerPar_p(NULL)
    {
    y2milestone( "Konstruktor SystemCmd UseTmp:%d Nr:%d", UseTmp_b, Nr_i );
    Append_ab[0] = false;
    Append_ab[1] = false;
    initFile();
    }

SystemCmd::~SystemCmd()
    {
    if( !Append_ab[IDX_STDOUT] && FileName_aC[IDX_STDOUT]!="" )
	{
  	unlink( FileName_aC[IDX_STDOUT].c_str() );
	}
    if( !Append_ab[IDX_STDERR] && FileName_aC[IDX_STDERR]!="" )
	{
  	unlink( FileName_aC[IDX_STDERR].c_str() );
	}
    }

void
SystemCmd::initFile( )
    {
    string Tmp_Ci = "/tmp";

    invalidate();
    Tmp_Ci += '/';
    Tmp_Ci += "YaST2.tdir";
    mkdir( Tmp_Ci.c_str(), 0700 );
    char buf[20];
    Tmp_Ci += "/stp";
    sprintf( buf, "%d", getpid() );
    Tmp_Ci += buf;
    Tmp_Ci += '_';
    sprintf( buf, "%d", Nr_i );
    Tmp_Ci += buf;
    Tmp_Ci += '_';
    Nr_i++;

    if( !Append_ab[IDX_STDOUT] )
	{
	if( FileName_aC[IDX_STDOUT].length() )
	    {
  	    unlink( FileName_aC[IDX_STDOUT].c_str() );
	    }
	FileName_aC[IDX_STDOUT] = Tmp_Ci+'S';
	unlink( FileName_aC[IDX_STDOUT].c_str() );
	}
    if( !Append_ab[IDX_STDERR] )
	{
	if( FileName_aC[IDX_STDERR].length() )
	    {
  	    unlink( FileName_aC[IDX_STDERR].c_str() );
	    }
	FileName_aC[IDX_STDERR] = Tmp_Ci+'E';
	unlink( FileName_aC[IDX_STDERR].c_str() );
	}
    y2milestone( "File:\"%s\"", FileName_aC[IDX_STDOUT].c_str() );
    }

void
SystemCmd::initCmd( string CmdIn_rv, string& CmdRedir_Cr )
    {
    invalidate();
    CmdRedir_Cr = CmdIn_rv;
    if( !Combine_b )
	{
	CmdRedir_Cr += " >";
	if( Append_ab[IDX_STDOUT] )
	    {
	    CmdRedir_Cr += ">";
	    }
	CmdRedir_Cr += FileName_aC[IDX_STDOUT];
	CmdRedir_Cr += " 2>";
	if( Append_ab[IDX_STDERR] )
	    {
	    CmdRedir_Cr += ">";
	    }
	CmdRedir_Cr += FileName_aC[IDX_STDERR];
	}
    else
	{
	CmdRedir_Cr += " >";
	if( Append_ab[IDX_STDOUT] )
	    {
	    CmdRedir_Cr += ">";
	    }
	CmdRedir_Cr += FileName_aC[IDX_STDOUT];
	CmdRedir_Cr += " 2>&1";
	}
    y2debug( "InitCmd:\"%s\"", CmdRedir_Cr.c_str() );
    }

void
SystemCmd::setOutputHandler( void (*Handle_f)( void *, string, bool ),
	                     void * Par_p )
    {
    OutputHandler_f = Handle_f;
    HandlerPar_p = Par_p;
    }

void 
SystemCmd::closeOpenFds()
    {
    int max_fd = getdtablesize();
    for( int fd = 3; fd < max_fd; fd++ ) 
	{
	close(fd);
	}
    }


#define ALTERNATE_SHELL "/bin/bash"

int
SystemCmd::executeBackground( const string& Cmd_Cv )
    {
    y2milestone( "SystemCmd Executing (Background):\"%s\"", Cmd_Cv.c_str() );
    Background_b = true;
    return( doExecute( Cmd_Cv ));
    }

int
SystemCmd::execute( const string& Cmd_Cv )
    {
    y2milestone( "SystemCmd Executing:\"%s\"", Cmd_Cv.c_str() );
    Background_b = false;
    return( doExecute( Cmd_Cv ));
    }

int
SystemCmd::doExecute( string Cmd_Cv )
    {
    string Cmd_Ci;
    string Shell_Ci = "/bin/sh";

    timeMark( "System", false );
    initCmd( Cmd_Cv, Cmd_Ci );
    y2debug( "Cmd_Ci:%s", Cmd_Ci.c_str() );
    if( access( Shell_Ci.c_str(), X_OK ) != 0 )
	{
	Shell_Ci = ALTERNATE_SHELL;
	}
    switch( (Pid_i=fork()) )
	{
	case 0:
	    setenv( "LC_ALL", "C", 1 );
	    setenv( "LANGUAGE", "C", 1 );
	    closeOpenFds();
	    Ret_i = execl( Shell_Ci.c_str(), Shell_Ci.c_str(), "-c",
			   Cmd_Ci.c_str(),
	                   NULL );
	    y2error( "SHOULD NOT HAPPEN \"%s\" Ret:%d", Shell_Ci.c_str(),
		     Ret_i );
	    break;
	case -1:
	    Ret_i = -1;
	    break;
	default:
	    Ret_i = 0;
	    if( !Background_b )
		{
		doWait( true, Ret_i );
		}
	    break;
	}
    timeMark( "After system()" );
    if( Ret_i==-127 || Ret_i==-1 )
	{
	y2error("system (%s) = %d", Cmd_Cv.c_str(), Ret_i);
	}
    openFiles();
    checkOutput();
    y2milestone( "system() Returns:%d", Ret_i );
    timeMark( "After CheckOutput" );
    return( Ret_i );
    }


bool
SystemCmd::doWait( bool Hang_bv, int& Ret_ir )
    {
    int Wait_ii;
    int Status_ii;

    Wait_ii = waitpid( Pid_i, &Status_ii, Hang_bv?0:WNOHANG );
    if( Wait_ii != 0 )
	{
	if( !WIFEXITED(Status_ii) )
	    {
	    Ret_ir = -127;
	    }
	else
	    {
	    Ret_ir = WEXITSTATUS(Status_ii);
	    }
	}
    y2debug( "Wait:%d pid=%d stat=%d Hang:%d Ret:%d", Wait_ii, Pid_i,
             Status_ii, Hang_bv, Ret_ir );
    return( Wait_ii != 0 );
    }

void
SystemCmd::setCombine( const bool Comb_bv )
    {
    Combine_b = Comb_bv;
    }

string
SystemCmd::getFilename( unsigned Idx_iv )
    {
    string Ret_Ci;

    if( Idx_iv > 1 )
	{
	y2error("invalid index %d", Idx_iv );
	}
    else
	{
	Ret_Ci = FileName_aC[Idx_iv];
	}
    return( Ret_Ci );
    }


void
SystemCmd::appendTo( string File_Cv, const unsigned Idx_iv )
    {
    struct statfs Buf_ri;
    string::size_type Pos_ii;

    if( Idx_iv > 1 )
	{
	y2error("invalid index %d", Idx_iv );
	}
    y2debug( "AppendTo File:\"%s\"", File_Cv.c_str() );
    if( !File_Cv.length() )
        {
        if( Append_ab[Idx_iv] )
	    {
	    Append_ab[Idx_iv] = false;
	    FileName_aC[Idx_iv] = "";
	    initFile();
	    }
        return;                      // <--- just reset and return
        }
    if( (Pos_ii=File_Cv.find( '/' )) != string::npos )
	{
	string Tmp_Ci( File_Cv );
	Tmp_Ci.erase(0, Pos_ii );
	if( access( Tmp_Ci.c_str(), R_OK ) )
	    {
	    y2debug( "AppendTo CreatePath:\"%s\"", Tmp_Ci.c_str() );
	    createPath( Tmp_Ci );
	    }
	}
    std::ofstream File_Ci( File_Cv.c_str(), std::ios::app );
    File_Ci.close();
    if( !Append_ab[Idx_iv] && FileName_aC[Idx_iv].length() )
	{
  	unlink( FileName_aC[Idx_iv].c_str() );
	}
    Append_ab[Idx_iv] = true;
    statfs( File_Cv.c_str(), &Buf_ri );
#ifdef SYSTEMCMD_VERBOSE_DEBUG
    y2debug( "Statfs File:\"%s\" Free:%d", File_Cv.c_str(), Buf_ri.f_bfree );
#endif
    if( Buf_ri.f_bfree > 0 )
	{
	FileName_aC[Idx_iv] = File_Cv;
	}
    else
	{
	y2error( "File System full - Append to /dev/null" );
	FileName_aC[Idx_iv] = "/dev/null";
	}
    }

void
SystemCmd::openFiles()
    {
    int Rest_ii = 5*1000000;

    File_aC[IDX_STDOUT].close();
    if( !Append_ab[IDX_STDOUT] )
	{
	while( access( FileName_aC[IDX_STDOUT].c_str(), R_OK ) == -1 && Rest_ii>0 )
	    {
	    delay( 10000 );
	    Rest_ii -= 10000;
	    }
	File_aC[IDX_STDOUT].clear();
	File_aC[IDX_STDOUT].open( FileName_aC[IDX_STDOUT].c_str() );
	if( !File_aC[IDX_STDOUT].good() )
	    {
	    y2error("could not open %s", FileName_aC[IDX_STDOUT].c_str() );
	    }
	}
    File_aC[IDX_STDERR].close();
    if( !Append_ab[IDX_STDERR] && !Combine_b )
	{
	while( access( FileName_aC[IDX_STDERR].c_str(), R_OK ) == -1 && Rest_ii>0 )
	    {
	    delay( 10000 );
	    Rest_ii -= 10000;
	    }
	File_aC[IDX_STDERR].clear();
	File_aC[IDX_STDERR].open( FileName_aC[IDX_STDERR].c_str() );
	if( !File_aC[IDX_STDERR].good() )
	    {
	    y2error("could not open %s", FileName_aC[IDX_STDERR].c_str() );
	    }
	}
    }


const string *
SystemCmd::getString( unsigned Idx_iv ) const
    {
    if( Idx_iv > 1 )
	{
	y2error("invalid index %d", Idx_iv );
	}
    if( !Valid_ab[Idx_iv] )
	{
	unsigned int I_ii;

	Text_aC[Idx_iv] = "";
	I_ii=0;
	while( I_ii<Lines_aC[Idx_iv].size() )
	    {
	    Text_aC[Idx_iv] += Lines_aC[Idx_iv][I_ii];
	    Text_aC[Idx_iv] += '\n';
	    I_ii++;
	    }
	Valid_ab[Idx_iv] = true;
	}
    return( &Text_aC[Idx_iv] );
    }

int
SystemCmd::numLines( bool Sel_bv, unsigned Idx_iv ) const
    {
    int Ret_ii;

    if( Idx_iv > 1 )
	{
	y2error("invalid index %d", Idx_iv );
	}
    if( Sel_bv )
	{
	Ret_ii = SelLines_aC[Idx_iv].size();
	}
    else
	{
	Ret_ii = Lines_aC[Idx_iv].size();
	}
    y2debug("ret:%d", Ret_ii );
    return( Ret_ii );
    }

const string *
SystemCmd::getLine( unsigned Nr_iv, bool Sel_bv, unsigned Idx_iv ) const
    {
    const string * Ret_pCi = NULL;

    if( Idx_iv > 1 )
	{
	y2error("invalid index %d", Idx_iv );
	}
    if( Sel_bv )
	{
	if( Nr_iv < SelLines_aC[Idx_iv].capacity() )
	    {
	    Ret_pCi = SelLines_aC[Idx_iv][Nr_iv];
	    }
	}
    else
	{
	if( Nr_iv < Lines_aC[Idx_iv].size() )
	    {
	    Ret_pCi = &Lines_aC[Idx_iv][Nr_iv];
	    }
	}
    return( Ret_pCi );
    }

int
SystemCmd::select( string Pat_Cv, bool Invert_bv, unsigned Idx_iv )
    {
    int I_ii;
    int End_ii;
    int Size_ii;
    string::size_type Pos_ii;
    bool BeginOfLine_bi;
    string Search_Ci( Pat_Cv );

    y2debug( "Select Idx:%d Pattern:\"%s\" Invert:%d", Idx_iv, Pat_Cv.c_str(), 
             Invert_bv );
    if( Idx_iv > 1 )
	{
	y2error("invalid index %d", Idx_iv );
	}
    BeginOfLine_bi = Search_Ci.length()>0 && Search_Ci[0]=='^';
    if( BeginOfLine_bi )
	{
	Search_Ci.erase( 0, 1 );
	}
    SelLines_aC[Idx_iv].resize(0);
    Size_ii = 0;
    End_ii = Lines_aC[Idx_iv].size();
    for( I_ii=0; I_ii<End_ii; I_ii++ )
	{
	Pos_ii = Lines_aC[Idx_iv][I_ii].find( Search_Ci );
	if( Pos_ii>0 && BeginOfLine_bi )
	    {
	    Pos_ii = string::npos;
	    }
	if( (Pos_ii != string::npos) != Invert_bv )
	    {
	    SelLines_aC[Idx_iv].resize( Size_ii+1 );
	    SelLines_aC[Idx_iv][Size_ii] = &Lines_aC[Idx_iv][I_ii];
	    y2debug( "Select Added Line %d \"%s\"", Size_ii, 
	             SelLines_aC[Idx_iv][Size_ii]->c_str() );
	    Size_ii++;
	    }
	}
    y2debug( "Select Lines %d", Size_ii );
    return( Size_ii );
    }

void
SystemCmd::invalidate()
    {
    int Idx_ii;

    for( Idx_ii=0; Idx_ii<2; Idx_ii++ )
	{
	Valid_ab[Idx_ii] = false;
	SelLines_aC[Idx_ii].resize(0);
	Lines_aC[Idx_ii].clear();
	NewLineSeen_ab[Idx_ii] = true;
	}
    }

void
SystemCmd::checkOutput()
    {
    struct statfs Buf_ri;
    bool Full_bi = false;
    bool Done_bi = false;
    string Name_Ci;

    do
	{
	if( Background_b && !(Done_bi=doWait( false, Ret_i )))
	    {
	    delay( 100000 );
	    }
	statfs( FileName_aC[IDX_STDOUT].c_str(), &Buf_ri );
#ifdef SYSTEMCMD_VERBOSE_DEBUG
	y2debug( "File:\"%s\" Free Stdout:%d", FileName_aC[IDX_STDOUT].c_str(),
	         Buf_ri.f_bfree );
#endif
	if( Buf_ri.f_bfree == 0 )
	    {
	    if( !Append_ab[IDX_STDOUT] )
		{
		y2error("Filesystem full!! Couldn't write to /tmp or /mnt/tmp");
		}
	    else
		{
		Full_bi = true;
		Name_Ci = FileName_aC[IDX_STDOUT];
		}
	    }
	if( !Append_ab[IDX_STDOUT] )
	    {
	    getUntilEOF( File_aC[IDX_STDOUT], Lines_aC[IDX_STDOUT],
			 NewLineSeen_ab[IDX_STDOUT], false );
	    }
	statfs( FileName_aC[IDX_STDERR].c_str(), &Buf_ri );
	if( Buf_ri.f_bfree == 0 && !Full_bi )
	    {
	    if( !Append_ab[IDX_STDERR] )
		{
		y2error("Filesystem full!! Couldn't write to /tmp or /mnt/tmp");
		}
	    else
		{
		Full_bi = true;
		Name_Ci = FileName_aC[IDX_STDERR];
		}
	    }
	if( !Append_ab[IDX_STDERR] )
	    {
	    getUntilEOF( File_aC[IDX_STDERR], Lines_aC[IDX_STDERR],
			 NewLineSeen_ab[IDX_STDERR], true );
	    }
	}
    while( !Full_bi && Background_b && !Done_bi );
    if( Full_bi )
	{
	string Tmp_Ci = string("TXT_ERR_FS_FULL") + Name_Ci;
	if( OutputHandler_f )
	    {
	    OutputHandler_f( HandlerPar_p, Tmp_Ci, true );
	    }
	Lines_aC[IDX_STDERR].push_back( Tmp_Ci );
	}
    }

#define BUF_LEN 256
#define MAX_STRING (STRING_MAXLEN - BUF_LEN - 10)

void
SystemCmd::getUntilEOF( std::ifstream& File_Cr, vector<string>& Lines_Cr,
                        bool& NewLine_br, bool Stderr_bv )
    {
    char Buf_ti[BUF_LEN];
    int Cnt_ii;
    int Char_ii;
    string Text_Ci;

    File_Cr.clear();
    Cnt_ii = 0;
    while( (Char_ii=File_Cr.get()) != EOF )
	{
	Buf_ti[Cnt_ii++] = Char_ii;
	if( Cnt_ii==sizeof(Buf_ti)-1 )
	    {
	    Buf_ti[Cnt_ii] = 0;
	    if( OutputHandler_f )
		{
#ifdef SYSTEMCMD_VERBOSE_DEBUG
		y2debug( "Calling Output-Handler Buf:\"%s\" Stderr:%d", Buf_ti,
		         Stderr_bv );
#endif
		OutputHandler_f( HandlerPar_p, Buf_ti, Stderr_bv );
		}
	    extractNewline( Buf_ti, Cnt_ii, NewLine_br, Text_Ci, Lines_Cr );
	    Cnt_ii = 0;
	    }
	}
    if( Cnt_ii>0 )
	{
	Buf_ti[Cnt_ii] = 0;
	if( OutputHandler_f )
	    {
#ifdef SYSTEMCMD_VERBOSE_DEBUG
	    y2debug( "Calling Output-Handler Buf:\"%s\" Stderr:%d",< Buf_ti
		     Stderr_bv );
#endif
	    OutputHandler_f( HandlerPar_p, Buf_ti, Stderr_bv );
	    }
	extractNewline( Buf_ti, Cnt_ii, NewLine_br, Text_Ci, Lines_Cr );
	}
    if( Text_Ci.length() > 0 )
	{
	addLine( Text_Ci, Lines_Cr );
	NewLine_br = false;
	}
    else
	{
	NewLine_br = true;
	}
    }

void
SystemCmd::extractNewline( char* Buf_ti, int Cnt_iv, bool& NewLine_br,
                           string& Text_Cr, vector<string>& Lines_Cr )
    {
    string::size_type Idx_ii;

    Text_Cr += Buf_ti;
    while( (Idx_ii=Text_Cr.find( '\n' )) != string::npos )
	{
	if( !NewLine_br )
	    {
	    Lines_Cr[Lines_Cr.size()-1] += Text_Cr.substr( 0, Idx_ii );
	    }
	else
	    {
	    addLine( Text_Cr.substr( 0, Idx_ii ), Lines_Cr );
	    }
	NewLine_br = true;
	Text_Cr.erase( 0, Idx_ii+1 );
	}
    }

void
SystemCmd::addLine( string Text_Cv, vector<string>& Lines_Cr )
    {
#ifndef FULL_DEBUG_SYSTEM_CMD
    if( Lines_Cr.size()<100 )
	{
#endif
	y2debug( "Adding Line %d \"%s\"", Lines_Cr.size()+1, Text_Cv.c_str() );
#ifndef FULL_DEBUG_SYSTEM_CMD
	}
#endif
    Lines_Cr.push_back( Text_Cv );
    }


///////////////////////////////////////////////////////////////////
//
//	METHOD NAME : SystemCmd::PlaceOutput
//	METHOD TYPE : int
//
//	INPUT  :
//	OUTPUT :
//	DESCRIPTION : Place stdout/stderr linewise in Ret_Cr
//
int SystemCmd::placeOutput( unsigned Which_iv, vector<string> &Ret_Cr, 
                            const bool Append_bv ) const
{
  if ( !Append_bv )
    Ret_Cr.clear();

  int Lines_ii = numLines( false, Which_iv );

  for ( int i_ii = 0; i_ii < Lines_ii; i_ii++ )
    Ret_Cr.push_back( *getLine( i_ii, false, Which_iv ) );

  return( Lines_ii );
}
