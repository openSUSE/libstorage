/*
 *  Author:	Arvin Schnell <aschnell@suse.de>
 */

/*
    Textdomain	"storage"
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


    list<string>
    getSuffix(int i, bool classic, bool sloppy = false)
    {
	list<string> ret;

	switch (i)
	{
	    case 0:
		/* symbol for "bytes" (best keep untranslated) */
		ret.push_back(classic ? "B" : _("B"));
		if (sloppy)
		    ret.push_back("");
		break;

	    case 1:
		/* symbol for "kilo bytes" (best keep untranslated) */
		ret.push_back(classic ? "kB" : _("kB"));
		/* symbol for "kibi bytes" (best keep untranslated) */
		ret.push_back(classic ? "KiB" : _("KiB"));
		if (sloppy)
		    /* symbol for "kilo" (best keep untranslated) */
		    ret.push_back(classic ? "k" : _("k"));
		break;

	    case 2:
		/* symbol for "mega bytes" (best keep untranslated) */
		ret.push_back(classic ? "MB" : _("MB"));
		/* symbol for "mebi bytes" (best keep untranslated) */
		ret.push_back(classic ? "MiB" : _("MiB"));
		if (sloppy)
		    /* symbol for "mega" (best keep untranslated) */
		    ret.push_back(classic ? "M" : _("M"));
		break;

	    case 3:
		/* symbol for "giga bytes" (best keep untranslated) */
		ret.push_back(classic ? "GB" : _("GB"));
		/* symbol for "gibi bytes" (best keep untranslated) */
		ret.push_back(classic ? "GiB" : _("GiB"));
		if (sloppy)
		    /* symbol for "giga" (best keep untranslated) */
		    ret.push_back(classic ? "G" : _("G"));
		break;

	    case 4:
		/* symbol for "tera bytes" (best keep untranslated) */
		ret.push_back(classic ? "TB" : _("TB"));
		/* symbol for "tebi bytes" (best keep untranslated) */
		ret.push_back(classic ? "TiB" : _("TiB"));
		if (sloppy)
		    /* symbol for "tera" (best keep untranslated) */
		    ret.push_back(classic ? "T" : _("T"));
		break;

	    case 5:
		/* symbol for "peta bytes" (best keep untranslated) */
		ret.push_back(classic ? "PB" : _("PB"));
		/* symbol for "pebi bytes" (best keep untranslated) */
		ret.push_back(classic ? "PiB" : _("PiB"));
		if (sloppy)
		    /* symbol for "peta" (best keep untranslated) */
		    ret.push_back(classic ? "P" : _("P"));
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

	s << f << ' ' << getSuffix(i, classic).front();

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
	    list<string> suffix = getSuffix(i, classic, !classic);

	    for (list<string>::const_iterator j = suffix.begin(); j != suffix.end(); ++j)
	    {
		if (boost::iends_with(str_trimmed, *j, loc))
		{
		    string number = str_trimmed.substr(0, str_trimmed.size() - j->size());

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
