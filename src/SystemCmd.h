// -*- C++ -*-
// Maintainer: fehr@suse.de

#ifndef _SystemCmd_h
#define _SystemCmd_h

#include <sys/poll.h>
#include <stdio.h>

#include <string>
#include <vector>

using std::vector;

class OutputProcessor;

class SystemCmd
    {
    public:
	enum OutputStream { IDX_STDOUT, IDX_STDERR };
	SystemCmd( const char* Command_Cv );
	SystemCmd( const string& Command_Cv );
	SystemCmd();
	virtual ~SystemCmd();
	int execute( const string& Command_Cv );
	int executeBackground( const string& Command_Cv );
	void setOutputHandler( void (*Handle_f)( void *, string, bool ),
	                       void * Par_p );
	void setOutputProcessor( OutputProcessor * proc )
	    { output_proc = proc; }
	int select( string Reg_Cv, bool Invert_bv=false,
	            unsigned Idx_ii=IDX_STDOUT );
	const string* getString( unsigned Idx_ii=IDX_STDOUT ) const;
	const string* getLine( unsigned Num_iv, bool Selected_bv=false,
			       unsigned Idx_ii=IDX_STDOUT ) const;
	int numLines( bool Selected_bv=false, unsigned Idx_ii=IDX_STDOUT ) const;
	void setCombine( const bool Combine_b=true );
	int retcode() const { return Ret_i; }

	int getStdout( vector<string> &Ret_Cr, const bool Append_bv = false ) const
	    { return placeOutput( IDX_STDOUT, Ret_Cr, Append_bv); }
	int getStderr( vector<string> &Ret_Cr, const bool Append_bv = false ) const
	    { return placeOutput( IDX_STDERR, Ret_Cr, Append_bv); }

    protected:

        int  placeOutput( unsigned Which_iv, vector<string> &Ret_Cr, const bool Append_bv ) const;

	void invalidate();
	void closeOpenFds();
	int doExecute( string Cmd_Cv );
	bool doWait( bool Hang_bv, int& Ret_ir );
        void checkOutput();
	void getUntilEOF( FILE* File_Cr, vector<string>& Lines_Cr,
	                  bool& NewLineSeen_br, bool Stderr_bv );
	void extractNewline( const char* Buf_ti, int Cnt_ii, bool& NewLineSeen_br,
	                     string& Text_Cr, vector<string>& Lines_Cr );
	void addLine( string Text_Cv, vector<string>& Lines_Cr );
	void init();

	mutable string Text_aC[2];
	mutable bool Valid_ab[2];
	FILE* File_aC[2];
	vector<string> Lines_aC[2];
	vector<string*> SelLines_aC[2];
	bool NewLineSeen_ab[2];
	bool Combine_b;
	bool Background_b;
	int Ret_i;
	int Pid_i;
	void (* OutputHandler_f)( void*, string, bool );
	void *HandlerPar_p;
	OutputProcessor* output_proc;
	struct pollfd pfds[2];
	static int Nr_i;
    };

#endif
