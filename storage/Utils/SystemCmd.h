/*
 * Copyright (c) [2004-2009] Novell, Inc.
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


#ifndef SYSTEM_CMD_H
#define SYSTEM_CMD_H

#include <sys/poll.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <list>
#include <boost/noncopyable.hpp>


namespace storage
{
    using std::string;
    using std::vector;

    class OutputProcessor;

    class SystemCmd : private boost::noncopyable
    {
    public:

	enum OutputStream { IDX_STDOUT, IDX_STDERR };

	SystemCmd(const string& Command_Cv);
	SystemCmd();

	virtual ~SystemCmd();

	int execute(const string& Command_Cv);
	int executeBackground(const string& Command_Cv);
	int executeRestricted(const string& Command_Cv,
			      unsigned long MaxTimeSec, unsigned long MaxLineOut,
			      bool& ExceedTime, bool& ExceedLines);

	void setOutputProcessor(OutputProcessor* proc) { output_proc = proc; }

	const vector<string>& stdout() const { return Lines_aC[IDX_STDOUT]; }
	const vector<string>& stderr() const { return Lines_aC[IDX_STDERR]; }

	string cmd() const { return lastCmd; }
	int retcode() const { return Ret_i; }

	int select(const string& Reg_Cv, OutputStream Idx_ii = IDX_STDOUT);
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
	void closeOpenFds() const;
	int doExecute(const string& Cmd_Cv);
	bool doWait(bool Hang_bv, int& Ret_ir);
	void checkOutput();
	void getUntilEOF(FILE* File_Cr, std::vector<string>& Lines_Cr,
			 bool& NewLineSeen_br, bool Stderr_bv);
	void extractNewline(const string& Buf_ti, int Cnt_ii, bool& NewLineSeen_br,
			    string& Text_Cr, std::vector<string>& Lines_Cr);
	void addLine(const string& Text_Cv, std::vector<string>& Lines_Cr);
	void init();

	void logOutput() const;

	FILE* File_aC[2];
	std::vector<string> Lines_aC[2];
	std::vector<string*> SelLines_aC[2];
	bool NewLineSeen_ab[2];
	bool Combine_b;
	bool Background_b;
	string lastCmd;
	int Ret_i;
	int Pid_i;
	OutputProcessor* output_proc;
	struct pollfd pfds[2];

	static bool testmode;

	static const unsigned line_limit = 50;
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
