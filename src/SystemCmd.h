// -*- C++ -*-
// Maintainer: fehr@suse.de

#ifndef _SystemCmd_h
#define _SystemCmd_h


#include <fstream>
#include <string>
#include <vector>

using std::vector;
using std::ifstream;

class SystemCmd
    {
    public:
	enum OutputStream { IDX_STDOUT, IDX_STDERR };
	SystemCmd( const char* Command_Cv,
	           bool UseTmp_bv=false );
	SystemCmd( const string& Command_Cv,
	           bool UseTmp_bv=false );
	SystemCmd( bool UseTmp_bv=false );
	virtual ~SystemCmd();
	int execute( const string& Command_Cv );
	int executeBackground( const string& Command_Cv );
	void setOutputHandler( void (*Handle_f)( void *, string, bool ),
	                       void * Par_p );
	int select( string Reg_Cv, bool Invert_bv=false,
	            unsigned Idx_ii=IDX_STDOUT );
	const string* getString( unsigned Idx_ii=IDX_STDOUT ) const;
	const string* getLine( unsigned Num_iv, bool Selected_bv=false,
			       unsigned Idx_ii=IDX_STDOUT ) const;
	int numLines( bool Selected_bv=false, unsigned Idx_ii=IDX_STDOUT ) const;
	void setCombine( const bool Combine_b=true );
	void appendTo( string File_Cv, unsigned Idx_ii=IDX_STDOUT );
	int retcode() const { return Ret_i; }
	string getFilename( unsigned Idx_ii=IDX_STDOUT );

	int getStdout( vector<string> &Ret_Cr, const bool Append_bv = false ) const
	    { return placeOutput( IDX_STDOUT, Ret_Cr, Append_bv); }
	int getStderr( vector<string> &Ret_Cr, const bool Append_bv = false ) const
	    { return placeOutput( IDX_STDERR, Ret_Cr, Append_bv); }

    protected:

        int  placeOutput( unsigned Which_iv, vector<string> &Ret_Cr, const bool Append_bv ) const;

	void invalidate();
	void initFile();
	void openFiles();
	void closeOpenFds();
	int doExecute( string Cmd_Cv );
	bool doWait( bool Hang_bv, int& Ret_ir );
        void initCmd( string CmdIn_rv, string& CmdRedir_Cr );
        void checkOutput();
	void getUntilEOF( ifstream& File_Cr, vector<string>& Lines_Cr,
	                  bool& NewLineSeen_br, bool Stderr_bv );
	void extractNewline( char* Buf_ti, int Cnt_ii, bool& NewLineSeen_br,
	                     string& Text_Cr, vector<string>& Lines_Cr );
	void addLine( string Text_Cv, vector<string>& Lines_Cr );

	string FileName_aC[2];
	mutable string Text_aC[2];
	mutable bool Valid_ab[2];
	ifstream File_aC[2];
	vector<string> Lines_aC[2];
	vector<string*> SelLines_aC[2];
	bool Append_ab[2];
	bool NewLineSeen_ab[2];
	bool Combine_b;
	bool UseTmp_b;
	bool Background_b;
	int Ret_i;
	int Pid_i;
	void (* OutputHandler_f)( void*, string, bool );
	void *HandlerPar_p;
	static int Nr_i;
    };

#endif
