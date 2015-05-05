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


#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <ostream>
#include <fstream>
#include <sys/wait.h>
#include <string>
#include <boost/algorithm/string.hpp>

#include "storage/Utils/AppUtil.h"
#include "storage/Utils/SystemCmd.h"
#include "storage/Utils/OutputProcessor.h"


namespace storage
{
    using namespace std;


    SystemCmd::SystemCmd( const string& command )
	: _combineOutput(false), _outputProc(NULL)
    {
	y2mil("constructor SystemCmd( \"" << command << "\" )");
	init();
	execute( command );
    }


    SystemCmd::SystemCmd()
	: _combineOutput(false), _outputProc(NULL)
    {
	y2mil("constructor SystemCmd()");
	init();
    }


    void SystemCmd::init()
    {
	_files[0] = _files[1] = NULL;
	_pfds[0].events = _pfds[1].events = POLLIN;
    }


    SystemCmd::~SystemCmd()
    {
	if ( _files[IDX_STDOUT] )
	    fclose( _files[IDX_STDOUT] );
	if ( _files[IDX_STDERR] )
	    fclose( _files[IDX_STDERR] );
    }


    void
    SystemCmd::closeOpenFds() const
    {
	int max_fd = getdtablesize();

	for ( int fd = 3; fd < max_fd; fd++ )
	{
	    close(fd);
	}
    }


    int
    SystemCmd::execute( const string& command )
    {
	y2mil("SystemCmd Executing: \"" << command << "\"");
	_execInBackground = false;
	return doExecute( command );
    }


    int
    SystemCmd::executeBackground( const string& command )
    {
	y2mil("SystemCmd Executing (Background): \"" << command << "\"");
	_execInBackground = true;
	return doExecute( command );
    }


    int
    SystemCmd::executeRestricted( const string& command,
				  long unsigned maxTimeSec, long unsigned maxLineOut,
				  bool& timeExceeded_ret, bool& linesExceeded_ret )
    {
	y2mil("cmd:" << command << " MaxTime:" << maxTimeSec << " MaxLines:" << maxLineOut);
	timeExceeded_ret = linesExceeded_ret = false;
	int ret = executeBackground( command );
	unsigned long ts = 0;
	unsigned long ls = 0;
	unsigned long start_time = time(NULL);
	while ( !timeExceeded_ret && !linesExceeded_ret && !doWait( false, ret ) )
	{
	    if ( maxTimeSec>0 )
	    {
		ts = time(NULL)-start_time;
		y2mil( "time used:" << ts );
	    }
	    if ( maxLineOut>0 )
	    {
		ls = numLines()+numLines(false,IDX_STDERR);
		y2mil( "lines out:" << ls );
	    }
	    timeExceeded_ret = maxTimeSec>0 && ts>maxTimeSec;
	    linesExceeded_ret = maxLineOut>0 && ls>maxLineOut;
	    sleep( 1 );
	}
	if ( timeExceeded_ret || linesExceeded_ret )
	{
	    int r = kill( _cmdPid, SIGKILL );
	    y2mil( "kill pid:" << _cmdPid << " ret:" << r );
	    unsigned count=0;
	    int cmdStatus;
	    int waitpidRet = -1;
	    while ( count<5 && waitpidRet<=0 )
	    {
		waitpidRet = waitpid( _cmdPid, &cmdStatus, WNOHANG );
		y2mil( "waitpid:" << waitpidRet );
		count++;
		sleep( 1 );
	    }
	    /*
	      r = kill( _cmdPid, SIGKILL );
	      y2mil( "kill pid:" << _cmdPid << " ret:" << r );
	      count=0;
	      waitDone = false;
	      while ( count<8 && !waitDone )
	      {
	          y2mil( "doWait:" << count );
	          waitDone = doWait( false, ret );
	          count++;
	          sleep( 1 );
	      }
	    */
	    _cmdRet = -257;
	}
	else
	    _cmdRet = ret;
	y2mil("ret:" << ret << " timeExceeded:" << timeExceeded_ret
	      << " linesExceeded_ret:" << linesExceeded_ret);
	return ret;
    }


#define PRIMARY_SHELL "/bin/sh"
#define ALTERNATE_SHELL "/bin/bash"

    int
    SystemCmd::doExecute( const string& command )
    {
	string shell = PRIMARY_SHELL;
	if ( access( shell.c_str(), X_OK ) != 0 )
	{
	    shell = ALTERNATE_SHELL;
	}

	_lastCmd = command;
	if ( _outputProc )
	{
	    _outputProc->reset();
	}
	y2deb("Cmd:" << command);

	StopWatch stopwatch;

	_files[IDX_STDERR] = _files[IDX_STDOUT] = NULL;
	invalidate();
	int sout[2];
	int serr[2];
	bool ok = true;
	if ( !_testmode && pipe(sout)<0 )
	{
	    y2err("pipe stdout creation failed errno:" << errno << " (" << strerror(errno) << ")");
	    ok = false;
	}
	if ( !_testmode && !_combineOutput && pipe(serr)<0 )
	{
	    y2err("pipe stderr creation failed errno:" << errno << " (" << strerror(errno) << ")");
	    ok = false;
	}
	if ( !_testmode && ok )
	{
	    _pfds[0].fd = sout[0];
	    if ( fcntl( _pfds[0].fd, F_SETFL, O_NONBLOCK )<0 )
	    {
		y2err("fcntl O_NONBLOCK failed errno:" << errno << " (" << strerror(errno) << ")");
	    }
	    if ( !_combineOutput )
	    {
		_pfds[1].fd = serr[0];
		if ( fcntl( _pfds[1].fd, F_SETFL, O_NONBLOCK )<0 )
		{
		    y2err("fcntl O_NONBLOCK failed errno:" << errno << " (" << strerror(errno) << ")");
		}
	    }
	    y2deb("sout:" << _pfds[0].fd << " serr:" << (_combineOutput?-1:_pfds[1].fd));
	    switch( (_cmdPid=fork()) )
	    {
		case 0:
		    setenv( "LC_ALL", "C", 1 );
		    setenv( "LANGUAGE", "C", 1 );
		    if ( dup2( sout[1], STDOUT_FILENO )<0 )
		    {
			y2err("dup2 stdout child failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    if ( !_combineOutput && dup2( serr[1], STDERR_FILENO )<0 )
		    {
			y2err("dup2 stderr child failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    if ( _combineOutput && dup2( STDOUT_FILENO, STDERR_FILENO )<0 )
		    {
			y2err("dup2 stderr child failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    if ( close( sout[0] )<0 )
		    {
			y2err("close child failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    if ( !_combineOutput && close( serr[0] )<0 )
		    {
			y2err("close child failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    closeOpenFds();
		    _cmdRet = execl( shell.c_str(), shell.c_str(), "-c",
				     command.c_str(), NULL );
		    // execl() should not return
		    y2err("execl() failed: THIS SHOULD NOT HAPPEN \"" << shell
			  << "\" Ret:" << _cmdRet << " errno: " << errno );
		    exit(127); // same as "command not found" in the shell
		    break;

		case -1:
		    _cmdRet = -1;
		    break;

		default:
		    if ( close( sout[1] )<0 )
		    {
			y2err("close parent failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    if ( !_combineOutput && close( serr[1] )<0 )
		    {
			y2err("close parent failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    _cmdRet = 0;
		    _files[IDX_STDOUT] = fdopen( sout[0], "r" );
		    if ( _files[IDX_STDOUT] == NULL )
		    {
			y2err("fdopen stdout failed errno:" << errno << " (" << strerror(errno) << ")");
		    }
		    if ( !_combineOutput )
		    {
			_files[IDX_STDERR] = fdopen( serr[0], "r" );
			if ( _files[IDX_STDERR] == NULL )
			{
			    y2err("fdopen stderr failed errno:" << errno << " (" << strerror(errno) << ")");
			}
		    }
		    if ( !_execInBackground )
		    {
			doWait( true, _cmdRet );
			y2mil("stopwatch " << stopwatch << " for \"" << cmd() << "\"");
		    }
		    break;
	    }
	}
	else if ( !_testmode )
	{
	    _cmdRet = -1;
	}
	else
	{
	    _cmdRet = 0;
	    y2mil("TESTMODE would execute \"" << command << "\"");
	}
	if ( _cmdRet==-127 || _cmdRet==-1 )
	{
	    y2err("system (\"" << command << "\") = " << _cmdRet);
	}
	if ( !_testmode )
	    checkOutput();
	y2mil("system() Returns:" << _cmdRet);
	if ( _cmdRet!=0 )
	    logOutput();
	return _cmdRet;
    }


    bool
    SystemCmd::doWait( bool hang, int& cmdRet_ret )
    {
	int waitpidRet;
	int cmdStatus;

	do
	{
	    y2deb("[0] id:" <<  _pfds[0].fd << " ev:" << hex << (unsigned)_pfds[0].events << dec << " [1] fs:" <<
		  (_combineOutput?-1:_pfds[1].fd) << " ev:" << hex << (_combineOutput?0:(unsigned)_pfds[1].events));
	    int sel = poll( _pfds, _combineOutput?1:2, 1000 );
	    if (sel < 0)
	    {
		y2err("poll failed errno:" << errno << " (" << strerror(errno) << ")");
	    }
	    y2deb("poll ret:" << sel);
	    if ( sel>0 )
	    {
		checkOutput();
	    }
	    waitpidRet = waitpid( _cmdPid, &cmdStatus, WNOHANG );
	    y2deb("Wait ret:" << waitpidRet);
	}
	while ( hang && waitpidRet == 0 );

	if ( waitpidRet != 0 )
	{
	    checkOutput();
	    fclose( _files[IDX_STDOUT] );
	    _files[IDX_STDOUT] = NULL;
	    if ( !_combineOutput )
	    {
		fclose( _files[IDX_STDERR] );
		_files[IDX_STDERR] = NULL;
	    }
	    if (WIFEXITED(cmdStatus))
	    {
		cmdRet_ret = WEXITSTATUS(cmdStatus);
		if (cmdRet_ret == 126)
		    y2err("command \"" << _lastCmd << "\" not executable");
		else if (cmdRet_ret == 127)
		    y2err("command \"" << _lastCmd << "\" not found");
	    }
	    else
	    {
		cmdRet_ret = -127;
		y2err("command \"" << _lastCmd << "\" failed");
	    }
	    if ( _outputProc )
	    {
		_outputProc->finish();
	    }
	}

	y2deb("Wait:" << waitpidRet << " pid:" << _cmdPid << " stat:" << cmdStatus <<
	      " Hang:" << hang << " Ret:" << cmdRet_ret);
	return waitpidRet != 0;
    }


    void
    SystemCmd::setCombine(bool val)
    {
	_combineOutput = val;
    }


    void
    SystemCmd::setTestmode(bool val)
    {
	_testmode = val;
    }


    unsigned
    SystemCmd::numLines( bool selected, OutputStream streamIndex ) const
    {
	unsigned lineCount;

	if ( streamIndex > 1 )
	{
	    y2err("invalid index " << streamIndex);
	}
	if ( selected )
	{
	    lineCount = _selectedOutputLines[streamIndex].size();
	}
	else
	{
	    lineCount = _outputLines[streamIndex].size();
	}
	y2deb("ret:" << lineCount);
	return lineCount;
    }


    string
    SystemCmd::getLine( unsigned lineNo, bool selected, OutputStream streamIndex ) const
    {
	string ret;

	if ( streamIndex > 1 )
	{
	    y2err("invalid index " << streamIndex);
	}
	if ( selected )
	{
	    if ( lineNo < _selectedOutputLines[streamIndex].capacity() )
	    {
		ret = *_selectedOutputLines[streamIndex][lineNo];
	    }
	}
	else
	{
	    if ( lineNo < _outputLines[streamIndex].size() )
	    {
		ret = _outputLines[streamIndex][lineNo];
	    }
	}
	return ret;
    }


    int
    SystemCmd::select( const string& pattern, OutputStream streamIndex )
    {
	if ( streamIndex > 1 )
	{
	    y2err("invalid index " << streamIndex);
	}
	string text( pattern );
	bool findAtStartOfLine = text.length()>0 && text[0]=='^';
	bool findAtEndOfLine = text.length()>0 && text[text.length()-1]=='$';
	if ( findAtStartOfLine )
	{
	    text.erase( 0, 1 );
	}
	if ( findAtEndOfLine )
	{
	    text.erase( text.length()-1, 1 );
	}
	_selectedOutputLines[streamIndex].resize(0);
	int hitCount = 0;
	int lineCount = _outputLines[streamIndex].size();

	for ( int i=0; i<lineCount; i++ )
	{
	    string::size_type pos = _outputLines[streamIndex][i].find( text );
	    if ( pos>0 && findAtStartOfLine )
	    {
		pos = string::npos;
	    }
	    if ( findAtEndOfLine &&
		pos!=(_outputLines[streamIndex][i].length()-text.length()) )
	    {
		pos = string::npos;
	    }
	    if (pos != string::npos)
	    {
		_selectedOutputLines[streamIndex].resize( hitCount+1 );
		_selectedOutputLines[streamIndex][hitCount] = &_outputLines[streamIndex][i];
		y2deb("Select Added Line " << hitCount << " \"" << *_selectedOutputLines[streamIndex][hitCount] << "\"");
		hitCount++;
	    }
	}

	y2mil("Pid:" << _cmdPid << " Idx:" << streamIndex << " SearchText:\"" << pattern << "\" Lines:" << hitCount);
	return hitCount;
    }


    void
    SystemCmd::invalidate()
    {
	for (int streamIndex = 0; streamIndex < 2; streamIndex++)
	{
	    _selectedOutputLines[streamIndex].resize(0);
	    _outputLines[streamIndex].clear();
	    _newLineSeen[streamIndex] = true;
	}
    }


    void
    SystemCmd::checkOutput()
    {
	y2deb("NewLine out:" << _newLineSeen[IDX_STDOUT] << " err:" << _newLineSeen[IDX_STDERR]);
	if (_files[IDX_STDOUT])
	    getUntilEOF(_files[IDX_STDOUT], _outputLines[IDX_STDOUT], _newLineSeen[IDX_STDOUT], false);
	if (_files[IDX_STDERR])
	    getUntilEOF(_files[IDX_STDERR], _outputLines[IDX_STDERR], _newLineSeen[IDX_STDERR], true);
	y2deb("NewLine out:" << _newLineSeen[IDX_STDOUT] << " err:" << _newLineSeen[IDX_STDERR]);
    }


#define BUF_LEN 256

    void
    SystemCmd::getUntilEOF( FILE* file, vector<string>& lines,
			    bool& newLineSeen_ret, bool isStderr ) const
    {
	size_t oldSize = lines.size();
	char buffer[BUF_LEN];
	int count;
	int c;
	string text;

	clearerr( file );
	count = 0;
	c = EOF;
	while ( (c=fgetc(file)) != EOF )
	{
	    buffer[count++] = c;
	    if ( count==sizeof(buffer)-1 )
	    {
		buffer[count] = 0;
		extractNewline( buffer, count, newLineSeen_ret, text, lines );
		count = 0;
		if ( _outputProc )
		{
		    _outputProc->process( buffer, isStderr );
		}
	    }
	    c = EOF;
	}
	if ( count>0 )
	{
	    buffer[count] = 0;
	    extractNewline( buffer, count, newLineSeen_ret, text, lines );
	    if ( _outputProc )
	    {
		_outputProc->process( buffer, isStderr );
	    }
	}
	if ( text.length() > 0 )
	{
	    if ( newLineSeen_ret )
	    {
		addLine( text, lines );
	    }
	    else
	    {
		lines[lines.size()-1] += text;
	    }
	    newLineSeen_ret = false;
	}
	else
	{
	    newLineSeen_ret = true;
	}
	y2deb("text:" << text << " NewLine:" << newLineSeen_ret);
	if ( oldSize != lines.size() )
	{
	    y2mil("pid:" << _cmdPid << " added lines:" << lines.size() - oldSize << " stderr:" << isStderr);
	}
    }


    void
    SystemCmd::extractNewline(const string& buffer, int count, bool& newLineSeen_ret,
			      string& text, vector<string>& lines) const
    {
	string::size_type index;

	text += buffer;
	while ( (index=text.find( '\n' )) != string::npos )
	{
	    if ( !newLineSeen_ret )
	    {
		lines[lines.size()-1] += text.substr( 0, index );
	    }
	    else
	    {
		addLine( text.substr( 0, index ), lines );
	    }
	    text.erase( 0, index+1 );
	    newLineSeen_ret = true;
	}
	y2deb("text: \"" << text << "\" newLineSeen: " << newLineSeen_ret);
    }


    void
    SystemCmd::addLine(const string& text, vector<string>& lines) const
    {
	if (lines.size() < LINE_LIMIT)
	{
	    y2mil("Adding Line " << lines.size() + 1 << " \"" << text << "\"");
	}
	else
	{
	    y2deb("Adding Line " << lines.size() + 1 << " \"" << text << "\"");
	}

	lines.push_back(text);
    }


    void
    SystemCmd::logOutput() const
    {
	unsigned lineCount = numLines(false, IDX_STDERR);
	if (lineCount <= LINE_LIMIT)
	{
	    for (unsigned i = 0; i < lineCount; ++i)
		y2mil("stderr:" << getLine(i, false, IDX_STDERR));
	}
	else
	{
	    for (unsigned i = 0; i < LINE_LIMIT / 2; ++i)
		y2mil("stderr:" << getLine(i, false, IDX_STDERR));
	    y2mil("stderr omitting lines");

	    for (unsigned i = lineCount - LINE_LIMIT / 2; i < lineCount; ++i)
		y2mil("stderr:" << getLine(i, false, IDX_STDERR));
	}

	lineCount = numLines(false, IDX_STDOUT);
	if (lineCount <= LINE_LIMIT)
	{
	    for (unsigned i = 0; i < lineCount; ++i)
		y2mil("stdout:" << getLine(i, false, IDX_STDOUT));
	}
	else
	{
	    for (unsigned i = 0; i < LINE_LIMIT / 2; ++i)
		y2mil("stdout:" << getLine(i, false, IDX_STDOUT));
	    y2mil("stdout omitting lines");

	    for (unsigned i = lineCount - LINE_LIMIT / 2; i < lineCount; ++i)
		y2mil("stdout:" << getLine(i, false, IDX_STDOUT));
	}
    }


    string
    SystemCmd::quote(const string& str)
    {
	return "'" + boost::replace_all_copy(str, "'", "'\\''") + "'";
    }


    string
    SystemCmd::quote(const list<string>& strs)
    {
	string ret;

	for (std::list<string>::const_iterator it = strs.begin(); it != strs.end(); it++)
	{
	    if (it != strs.begin())
		ret.append(" ");
	    ret.append(quote(*it));
	}
	return ret;
    }


    bool SystemCmd::_testmode = false;

}
