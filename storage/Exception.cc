/*
 * Copyright (c) [2014-2015] Novell, Inc.
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


/*
 * Large parts stolen from libyui/YUIException.h
 */

#include <sstream>
#include <string.h>	// strerror()
#include <stdio.h>

#include "storage/Exception.h"
#include "storage/Utils/AppUtil.h"


namespace storage
{
    std::string CodeLocation::asString() const
    {
	std::string str( _file );
	str += "(" + _func + "):";

	char formatted_number[ 20 ];
	sprintf( formatted_number, "%u", _line );

	str += formatted_number;

	return str;
    }


    std::ostream &
    operator<<( std::ostream & str, const CodeLocation & obj )
    {
	return str << obj.asString();
    }


    Exception::Exception()
    {
	// NOP
    }

    Exception::Exception( const std::string & msg_r )
	: _msg( msg_r )
    {
	// NOP
    }


    Exception::~Exception() throw()
    {
	// NOP
    }


    std::string
    Exception::asString() const
    {
	std::ostringstream str;
	dumpOn( str );
	return str.str();
    }


    std::ostream &
    Exception::dumpOn( std::ostream & str ) const
    {
	return str << _msg;
    }


    std::ostream &
    Exception::dumpError( std::ostream & str ) const
    {
	return dumpOn( str << _where << ": " );
    }


    std::ostream &
    operator<<( std::ostream & str, const Exception & obj )
    {
	return obj.dumpError( str );
    }


    std::string
    Exception::strErrno( int errno_r )
    {
	return strerror( errno_r );
    }


    std::string
    Exception::strErrno( int errno_r, const std::string & msg )
    {
	std::string ret( msg );
	ret += ": ";
	return ret += strErrno( errno_r );
    }


    void
    Exception::log( const Exception & 	 exception,
		    const CodeLocation & location,
		    const char * const 	 prefix )
    {
	y2log_op( storage::WARNING,
		  location.file().c_str(),
		  location.line(),
		  location.func().c_str(),
		  prefix << " " << exception.asString() );
    }

}
