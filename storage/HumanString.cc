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


#include <locale>
#include <boost/algorithm/string.hpp>

#include "AppUtil.h"
#include "HumanString.h"


namespace storage
{
    using namespace std;


    int
    numSuffixes()
    {
	return 6;
    }


    list<Text>
    getSuffix(int i, bool sloppy = false)
    {
	list<Text> ret;

	switch (i)
	{
	    case 0:
		/* symbol for "bytes" (best keep untranslated) */
		ret.push_back(_("B"));
		if (sloppy)
		    ret.push_back(Text("", ""));
		break;

	    case 1:
		/* symbol for "kilo bytes" (best keep untranslated) */
		ret.push_back(_("kB"));
		/* symbol for "kibi bytes" (best keep untranslated) */
		ret.push_back(_("KiB"));
		if (sloppy)
		    /* symbol for "kilo" (best keep untranslated) */
		    ret.push_back(_("k"));
		break;

	    case 2:
		/* symbol for "mega bytes" (best keep untranslated) */
		ret.push_back(_("MB"));
		/* symbol for "mebi bytes" (best keep untranslated) */
		ret.push_back(_("MiB"));
		if (sloppy)
		    /* symbol for "mega" (best keep untranslated) */
		    ret.push_back(_("M"));
		break;

	    case 3:
		/* symbol for "giga bytes" (best keep untranslated) */
		ret.push_back(_("GB"));
		/* symbol for "gibi bytes" (best keep untranslated) */
		ret.push_back(_("GiB"));
		if (sloppy)
		    /* symbol for "giga" (best keep untranslated) */
		    ret.push_back(_("G"));
		break;

	    case 4:
		/* symbol for "tera bytes" (best keep untranslated) */
		ret.push_back(_("TB"));
		/* symbol for "tebi bytes" (best keep untranslated) */
		ret.push_back(_("TiB"));
		if (sloppy)
		    /* symbol for "tera" (best keep untranslated) */
		    ret.push_back(_("T"));
		break;

	    case 5:
		/* symbol for "peta bytes" (best keep untranslated) */
		ret.push_back(_("PB"));
		/* symbol for "pebi bytes" (best keep untranslated) */
		ret.push_back(_("PiB"));
		if (sloppy)
		    /* symbol for "peta" (best keep untranslated) */
		    ret.push_back(_("P"));
		break;
	}

	return ret;
    }


    string
    byteToHumanString(unsigned long long size, bool classic, int precision, bool omit_zeroes)
    {
	const locale loc = classic ? locale::classic() : locale();

	double f = size;
	int i = 0;

	while (f >= 1024.0 && i + 1 < numSuffixes())
	{
	    f /= 1024.0;
	    i++;
	}

	if (omit_zeroes && (f == (unsigned long long)(f)))
	{
	    precision = 0;
	}

	ostringstream s;
	s.imbue(loc);
	s.setf(ios::fixed);
	s.precision(precision);

	if (classic)
	    s << f << ' ' << getSuffix(i).front().native;
	else
	    s << f << ' ' << getSuffix(i).front().text;

	return s.str();
    }


    bool
    humanStringToByte(const string& str, bool classic, unsigned long long& size)
    {
	const locale loc = classic ? locale::classic() : locale();

	const string str_trimmed = boost::trim_copy(str, loc);

	double f = 1.0;

	for (int i = 0; i < numSuffixes(); i++)
	{
	    list<Text> suffix = getSuffix(i, !classic);

	    for (list<Text>::const_iterator j = suffix.begin(); j != suffix.end(); ++j)
	    {
		const string& tmp = classic ? j->native : j->text;

		if (boost::iends_with(str_trimmed, tmp, loc))
		{
		    string number = str_trimmed.substr(0, str_trimmed.size() - tmp.size());

		    istringstream s(boost::trim_copy(number, loc));
		    s.imbue(loc);

		    double g;
		    s >> g;

		    if (!s.fail() && s.eof() && g >= 0.0)
		    {
			size = g * f;
			return true;
		    }
		}
	    }

	    f *= 1024.0;
	}

	return false;
    }

}
