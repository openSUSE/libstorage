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
	SystemCmd( const string& Command_Cv,
	           bool UseTmp_bv=false );
	SystemCmd( bool UseTmp_bv=false );
	virtual ~SystemCmd();
	int Execute( const string& Command_Cv );
	int ExecuteBackground( const string& Command_Cv );
	void SetOutputHandler( void (*Handle_f)( void *, string, bool ),
	                       void * Par_p );
	int Select( string Reg_Cv, bool Invert_bv=false,
	            unsigned Idx_ii=IDX_STDOUT );
	const string* GetString( unsigned Idx_ii=IDX_STDOUT ) const;
	const string* GetLine( unsigned Num_iv, bool Selected_bv=false,
				    unsigned Idx_ii=IDX_STDOUT ) const;
	int NumLines( bool Selected_bv=false, unsigned Idx_ii=IDX_STDOUT ) const;
	void SetCombine( const bool Combine_b=true );
	void AppendTo( string File_Cv, unsigned Idx_ii=IDX_STDOUT );
	int Retcode() const { return Ret_i; }
	string GetFilename( unsigned Idx_ii=IDX_STDOUT );

	int GetStdout( vector<string> &Ret_Cr, const bool Append_bv = false ) const
	    { return PlaceOutput( IDX_STDOUT, Ret_Cr, Append_bv); }
	int GetStderr( vector<string> &Ret_Cr, const bool Append_bv = false ) const
	    { return PlaceOutput( IDX_STDERR, Ret_Cr, Append_bv); }

    protected:

        int  PlaceOutput( unsigned Which_iv, vector<string> &Ret_Cr, const bool Append_bv ) const;

	void Invalidate();
	void InitFile();
	void OpenFiles();
	int DoExecute( string Cmd_Cv );
	bool DoWait( bool Hang_bv, int& Ret_ir );
        void InitCmd( string CmdIn_rv, string& CmdRedir_Cr );
        void CheckOutput();
	void GetUntilEOF( ifstream& File_Cr, vector<string>& Lines_Cr,
	                  bool& NewLineSeen_br, bool Stderr_bv );
	void ExtractNewline( char* Buf_ti, int Cnt_ii, bool& NewLineSeen_br,
	                     string& Text_Cr, vector<string>& Lines_Cr );
	void AddLine( string Text_Cv, vector<string>& Lines_Cr );

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
