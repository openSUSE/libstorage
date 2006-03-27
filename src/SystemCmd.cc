// Maintainer: fehr@suse.de
/*
  Textdomain    "storage"
*/

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sys/wait.h>

#include <string>

#include "y2storage/AppUtil.h"
#include "y2storage/SystemCmd.h"
#include "y2storage/OutputProcessor.h"

using namespace std;
using namespace storage;

int SystemCmd::Nr_i = 0;

//#define FULL_DEBUG_SYSTEM_CMD

SystemCmd::SystemCmd( const char* Command )
    {
    y2milestone( "Konstruktor SystemCmd:\"%s\" Nr:%d", Command, Nr_i );
    init();
    execute( Command );
    }

SystemCmd::SystemCmd( const string& Command_Cv )
    {
    y2milestone( "Konstruktor SystemCmd:\"%s\" Nr:%d", Command_Cv.c_str(), 
                 Nr_i );
    init();
    execute( Command_Cv );
    }

SystemCmd::SystemCmd()
    {
    init();
    y2milestone( "Konstruktor SystemCmd Nr:%d", Nr_i );
    }

void SystemCmd::init()
    {
    Combine_b = false;
    OutputHandler_f = NULL;
    HandlerPar_p = NULL;
    output_proc = NULL;
    File_aC[0] = File_aC[1] = NULL;
    pfds[0].events = pfds[1].events = POLLIN;
    }

SystemCmd::~SystemCmd()
    {
    if( File_aC[IDX_STDOUT] )
	fclose( File_aC[IDX_STDOUT] );
    if( File_aC[IDX_STDERR] )
	fclose( File_aC[IDX_STDERR] );
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
SystemCmd::doExecute( string Cmd )
    {
    string Shell_Ci = "/bin/sh";

    lastCmd = Cmd;
    if( output_proc )
	{
	output_proc->reset();
	}
    timeMark( "System", false );
    y2debug( "Cmd:%s", Cmd.c_str() );
    if( access( Shell_Ci.c_str(), X_OK ) != 0 )
	{
	Shell_Ci = ALTERNATE_SHELL;
	}
    File_aC[IDX_STDERR] = File_aC[IDX_STDOUT] = NULL;
    invalidate();
    int sout[2];
    int serr[2];
    bool ok_bi = true;
    if( !system_cmd_testmode && pipe(sout)<0 )
	{
	y2error( "pipe stdout creation failed errno=%d (%s)", errno, 
	         strerror(errno)); 
	ok_bi = false;
	}
    if( !system_cmd_testmode && !Combine_b && pipe(serr)<0 ) 
	{
	y2error( "pipe stderr creation failed errno=%d (%s)", errno, 
	         strerror(errno)); 
	ok_bi = false;
	}
    if( ok_bi && !system_cmd_testmode )
	{
	pfds[0].fd = sout[0];
	if( fcntl( pfds[0].fd, F_SETFL, O_NONBLOCK )<0 )
	    {
	    y2error( "fcntl O_NONBLOCK failed errno=%d (%s)", errno, 
		     strerror(errno)); 
	    }
	if( !Combine_b )
	    {
	    pfds[1].fd = serr[0];
	    if( fcntl( pfds[1].fd, F_SETFL, O_NONBLOCK )<0 )
		{
		y2error( "fcntl O_NONBLOCK failed errno=%d (%s)", errno, 
			 strerror(errno)); 
		}
	    }
	y2debug( "sout:%d serr:%d", pfds[0].fd, Combine_b?-1:pfds[1].fd );
	switch( (Pid_i=fork()) )
	    {
	    case 0:
		setenv( "LC_ALL", "C", 1 );
		setenv( "LANGUAGE", "C", 1 );
		if( dup2( sout[1], 1 )<0 )
		    {
		    y2error( "dup2 stdout child failed errno=%d (%s)", errno, 
			     strerror(errno));
		    }
		if( !Combine_b && dup2( serr[1], 2 )<0 )
		    {
		    y2error( "dup2 stderr child failed errno=%d (%s)", errno, 
			     strerror(errno));
		    }
		if( Combine_b && dup2( 1, 2 )<0 )
		    {
		    y2error( "dup2 stderr child failed errno=%d (%s)", errno, 
			     strerror(errno));
		    }
		if( close( sout[0] )<0 )
		    {
		    y2error( "close child failed errno=%d (%s)", errno, 
		             strerror(errno)); 
		    }
		if( !Combine_b && close( serr[0] )<0 )
		    {
		    y2error( "close child failed errno=%d (%s)", errno, 
		             strerror(errno)); 
		    }
		closeOpenFds();
		Ret_i = execl( Shell_Ci.c_str(), Shell_Ci.c_str(), "-c",
			       Cmd.c_str(), NULL );
		y2error( "SHOULD NOT HAPPEN \"%s\" Ret:%d", Shell_Ci.c_str(),
			 Ret_i );
		break;
	    case -1:
		Ret_i = -1;
		break;
	    default:
		if( close( sout[1] )<0 )
		    {
		    y2error( "close parent failed errno=%d (%s)", errno, 
		             strerror(errno)); 
		    }
		if( !Combine_b && close( serr[1] )<0 )
		    {
		    y2error( "close parent failed errno=%d (%s)", errno, 
		             strerror(errno)); 
		    }
		Ret_i = 0;
		File_aC[IDX_STDOUT] = fdopen( sout[0], "r" );
		if( File_aC[IDX_STDOUT] == NULL )
		    {
		    y2error( "fdopen stdout failed errno=%d (%s)", errno, 
		             strerror(errno)); 
		    }
		if( !Combine_b )
		    {
		    File_aC[IDX_STDERR] = fdopen( serr[0], "r" );
		    if( File_aC[IDX_STDERR] == NULL )
			{
			y2error( "fdopen stderr failed errno=%d (%s)", errno, 
				 strerror(errno)); 
			}
		    }
		if( !Background_b )
		    {
		    doWait( true, Ret_i );
		    }
		break;
	    }
	}
    else if( !system_cmd_testmode )
	{
	Ret_i = -1;
	}
    else
	{
	Ret_i = 0;
	y2milestone( "TESTMODE would execute \"%s\"", Cmd.c_str() );
	}
    timeMark( "After fork()" );
    if( Ret_i==-127 || Ret_i==-1 )
	{
	y2error("system (%s) = %d", Cmd.c_str(), Ret_i);
	}
    if( !system_cmd_testmode )
	checkOutput();
    y2milestone( "system() Returns:%d", Ret_i );
    if( Ret_i!=0 )
	logOutput();
    timeMark( "After CheckOutput" );
    return( Ret_i );
    }


bool
SystemCmd::doWait( bool Hang_bv, int& Ret_ir )
    {
    int Wait_ii;
    int Status_ii;
    int sel;

    do
	{
	y2debug( "[0] id:%d ev:%x [1] fs:%d ev:%x", 
	             pfds[0].fd, (unsigned)pfds[0].events,
		     Combine_b?-1:pfds[1].fd, Combine_b?0:(unsigned)pfds[1].events );
	if( (sel=poll( pfds, Combine_b?1:2, 1000 ))<0 )
	    {
	    y2error( "poll failed errno=%d (%s)", errno, strerror(errno)); 
	    }
	y2debug( "poll ret:%d", sel );
	if( sel>0 )
	    {
	    checkOutput();
	    }
	Wait_ii = waitpid( Pid_i, &Status_ii, WNOHANG );
	y2debug( "Wait ret:%d", Wait_ii );
	}
    while( Hang_bv && Wait_ii == 0 );
    if( Wait_ii != 0 )
	{
	checkOutput();
	fclose( File_aC[IDX_STDOUT] );
	File_aC[IDX_STDOUT] = NULL;
	if( !Combine_b )
	    {
	    fclose( File_aC[IDX_STDERR] );
	    File_aC[IDX_STDERR] = NULL;
	    }
	if( !WIFEXITED(Status_ii) )
	    {
	    Ret_ir = -127;
	    }
	else
	    {
	    Ret_ir = WEXITSTATUS(Status_ii);
	    }
	if( output_proc )
	    {
	    output_proc->finished();
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

unsigned
SystemCmd::numLines( bool Sel_bv, unsigned Idx_iv ) const
    {
    unsigned Ret_ii;

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
    y2debug("ret:%u", Ret_ii );
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
    y2milestone( "Pid:%d Idx:%d Pattern:\"%s\" Invert:%d Lines %d", Pid_i, 
                 Idx_iv, Pat_Cv.c_str(), Invert_bv, Size_ii );
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
    y2debug( "NewLine out:%d err:%d", NewLineSeen_ab[IDX_STDOUT],
	     NewLineSeen_ab[IDX_STDERR] );
    if( File_aC[IDX_STDOUT] )
	getUntilEOF( File_aC[IDX_STDOUT], Lines_aC[IDX_STDOUT],
		     NewLineSeen_ab[IDX_STDOUT], false );
    if( File_aC[IDX_STDERR] )
	getUntilEOF( File_aC[IDX_STDERR], Lines_aC[IDX_STDERR],
		     NewLineSeen_ab[IDX_STDERR], true );
    y2debug( "NewLine out:%d err:%d", NewLineSeen_ab[IDX_STDOUT],
	     NewLineSeen_ab[IDX_STDERR] );
    }

#define BUF_LEN 256
#define MAX_STRING (STRING_MAXLEN - BUF_LEN - 10)

void
SystemCmd::getUntilEOF( FILE* File_Cr, vector<string>& Lines_Cr,
                        bool& NewLine_br, bool Stderr_bv )
    {
    size_t old_size = Lines_Cr.size();
    char Buf_ti[BUF_LEN];
    int Cnt_ii;
    int Char_ii;
    string Text_Ci;

    clearerr( File_Cr );
    Cnt_ii = 0;
    Char_ii = EOF;
    while( (Char_ii=fgetc(File_Cr)) != EOF )
	{
	Buf_ti[Cnt_ii++] = Char_ii;
	if( Cnt_ii==sizeof(Buf_ti)-1 )
	    {
	    Buf_ti[Cnt_ii] = 0;
	    extractNewline( Buf_ti, Cnt_ii, NewLine_br, Text_Ci, Lines_Cr );
	    Cnt_ii = 0;
	    if( output_proc )
		{
		output_proc->process( Buf_ti, Stderr_bv );
		}
	    if( OutputHandler_f )
		{
#ifdef SYSTEMCMD_VERBOSE_DEBUG
		y2debug( "Calling Output-Handler Buf:\"%s\" Stderr:%d", Buf_ti,
		         Stderr_bv );
#endif
		OutputHandler_f( HandlerPar_p, Buf_ti, Stderr_bv );
		}
	    }
	Char_ii = EOF;
	}
    if( Cnt_ii>0 )
	{
	Buf_ti[Cnt_ii] = 0;
	extractNewline( Buf_ti, Cnt_ii, NewLine_br, Text_Ci, Lines_Cr );
	if( output_proc )
	    {
	    output_proc->process( Buf_ti, Stderr_bv );
	    }
	if( OutputHandler_f )
	    {
#ifdef SYSTEMCMD_VERBOSE_DEBUG
	    y2debug( "Calling Output-Handler Buf:\"%s\" Stderr:%d",< Buf_ti
		     Stderr_bv );
#endif
	    OutputHandler_f( HandlerPar_p, Buf_ti, Stderr_bv );
	    }
	}
    if( Text_Ci.length() > 0 )
	{
	if( NewLine_br )
	    {
	    addLine( Text_Ci, Lines_Cr );
	    }
	else
	    {
	    Lines_Cr[Lines_Cr.size()-1] += Text_Ci;
	    }
	NewLine_br = false;
	}
    else
	{
	NewLine_br = true;
	}
    y2debug( "Text_Ci:%s NewLine:%d", Text_Ci.c_str(), NewLine_br );
    if( old_size != Lines_Cr.size() )
	{
	y2milestone( "pid:%d added lines:%zd stderr:%d", Pid_i,
	             Lines_Cr.size()-old_size, Stderr_bv );
	if( Lines_Cr.size()>0 )
	    {
	    y2mil( "last line:\"" << Lines_Cr.back() << '"' );
	    }
	}
    }

void
SystemCmd::extractNewline( const char* Buf_ti, int Cnt_iv, bool& NewLine_br,
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
	Text_Cr.erase( 0, Idx_ii+1 );
	NewLine_br = true;
	}
    y2debug( "Text_Ci:%s NewLine:%d", Text_Cr.c_str(), NewLine_br );
    }

void
SystemCmd::addLine( string Text_Cv, vector<string>& Lines_Cr )
    {
#ifndef FULL_DEBUG_SYSTEM_CMD
    if( Lines_Cr.size()<100 )
	{
#endif
	y2milestone( "Adding Line %zd \"%s\"", Lines_Cr.size()+1, Text_Cv.c_str() );
#ifndef FULL_DEBUG_SYSTEM_CMD
	}
#endif
    Lines_Cr.push_back( Text_Cv );
    }

void
SystemCmd::logOutput()
    {
    for( unsigned i=0; i<numLines(false,IDX_STDERR); ++i )
	y2milestone( "stderr:%s", getLine( i, false, IDX_STDERR )->c_str() );
    for( unsigned i=0; i<numLines(); ++i )
	y2milestone( "stderr:%s", getLine( i )->c_str() );
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

int SystemCmd::placeOutput( unsigned Which_iv, list<string> &Ret_Cr,
                            const bool Append_bv ) const
{
  if ( !Append_bv )
    Ret_Cr.clear();

  int Lines_ii = numLines( false, Which_iv );

  for ( int i_ii = 0; i_ii < Lines_ii; i_ii++ )
    Ret_Cr.push_back( *getLine( i_ii, false, Which_iv ) );

  return( Lines_ii );
}
