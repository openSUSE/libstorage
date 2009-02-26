#ifndef SYSTEM_CMD_H
#define SYSTEM_CMD_H

#include <sys/poll.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <list>

using std::string;

namespace storage
{

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
	int executeRestricted( const string& Command_Cv,
	                       unsigned long MaxTimeSec, unsigned long MaxLineOut,
			       bool& ExceedTime, bool& ExceedLines);

	void setOutputHandler( void (*Handle_f)( void *, string, bool ), void * Par_p );
	void setOutputProcessor( OutputProcessor * proc ) { output_proc = proc; }
	void logOutput() const;

	string stderr() const { return getString(IDX_STDERR); }
	string stdout() const { return getString(IDX_STDOUT); }
	string cmd() const { return lastCmd; }
	int retcode() const { return Ret_i; }

	int select(string Reg_Cv, bool Invert_bv = false, OutputStream Idx_ii = IDX_STDOUT);
	unsigned numLines(bool Selected_bv = false, OutputStream Idx_ii = IDX_STDOUT) const;
	string getLine(unsigned Num_iv, bool Selected_bv = false, OutputStream Idx_ii = IDX_STDOUT) const;

	void setCombine(bool combine = true);

	static void setTestmode(bool testmode = true);

	/**
	 * Quotes and protects a single string for shell execution.
	 */
	static string quote(const string& str);

	/**
	 * Quotes and protects every single string in the list for shell execution.
	 */
	static string quote(const std::list<string>& strs);

    protected:

	void invalidate();
	void closeOpenFds();
	int doExecute( string Cmd_Cv );
	bool doWait( bool Hang_bv, int& Ret_ir );
        void checkOutput();
	void getUntilEOF( FILE* File_Cr, std::vector<string>& Lines_Cr,
	                  bool& NewLineSeen_br, bool Stderr_bv );
	void extractNewline( const char* Buf_ti, int Cnt_ii, bool& NewLineSeen_br,
	                     string& Text_Cr, std::vector<string>& Lines_Cr );
	void addLine( string Text_Cv, std::vector<string>& Lines_Cr );
	void init();

	string getString(OutputStream Idx_ii = IDX_STDOUT) const;

	FILE* File_aC[2];
	std::vector<string> Lines_aC[2];
	std::vector<string*> SelLines_aC[2];
	bool NewLineSeen_ab[2];
	bool Combine_b;
	bool Background_b;
	string lastCmd;
	int Ret_i;
	int Pid_i;
	void (* OutputHandler_f)( void*, string, bool );
	void *HandlerPar_p;
	OutputProcessor* output_proc;
	struct pollfd pfds[2];

	static bool testmode;

    };


    inline string quote(const string& str)
    {
	return SystemCmd::quote(str);
    }

    inline string quote(const std::list<string>& strs)
    {
	return SystemCmd::quote(strs);
    }

}

#endif
