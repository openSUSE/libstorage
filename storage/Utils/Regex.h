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


#ifndef REGEX_H
#define REGEX_H


#include <regex.h>
#include <string>
#include <stdexcept>
#include <boost/noncopyable.hpp>


namespace storage
{
    using std::string;


    class regex_error : public std::runtime_error
    {
    public:
	regex_error() : std::runtime_error("regex error") {}
    };


    class Regex : private boost::noncopyable
    {
    public:

	Regex (const char* pattern, int cflags = REG_EXTENDED, unsigned int = 10);
	Regex (const string& pattern, int cflags = REG_EXTENDED, unsigned int = 10);
	~Regex ();

	string getPattern () const { return pattern; }
	int getCflags () const { return cflags; }

	bool match (const string&, int eflags = 0) const;

	regoff_t so (unsigned int) const;
	regoff_t eo (unsigned int) const;

	string cap (unsigned int) const;

	static const string ws;
	static const string number;

	static string escape(const string& str); // only for REG_EXTENDED

    private:

	const string pattern;
	const int cflags;
	const unsigned int nm;

	mutable regex_t rx;
	mutable int my_nl_msg_cat_cntr;
	mutable regmatch_t* rm;

	mutable string last_str;
    };

}

#endif
