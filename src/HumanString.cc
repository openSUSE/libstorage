/*
 *  Author: Arvin Schnell <aschnell@suse.de>
 */

/*
  Textdomain    "storage"
*/


#include <locale>
#include <boost/algorithm/string.hpp>

#include "AppUtil.h"
#include "HumanString.h"


using namespace std;


namespace storage
{
    
    int
    numSuffixes()
    {
	return 6;
    }
    
    
    string
    getSuffix(int i, bool classic, bool sloppy = false)
    {
	switch (i)
	{
	    case 0:
		if (sloppy)
		    return "";
		else
		    /* Byte abbreviated */
		    return classic ? "B" : _("B");
		
	    case 1:
		if (sloppy)
		    /* Kilo abbreviated */
		    return classic ? "k" : _("k");
		else
		    /* KiloByte abbreviated */
		    return classic ? "kB" : _("kB");
		
	    case 2:
		if (sloppy)
		    /* Mega abbreviated */
		    return classic ? "M" : _("M");
		else
		    /* MegaByte abbreviated */
		    return classic ? "MB" : _("MB");
		
	    case 3:
		if (sloppy)
		    /* Giga abbreviated */
		    return classic ? "G" : _("G");
		else
		    /* GigaByte abbreviated */
		    return classic ? "GB" : _("GB");
		
	    case 4:
		if (sloppy)
		    /* Tera abbreviated */
		    return classic ? "T" : _("T");
		else
		    /* TeraByte abbreviated */
		    return classic ? "TB" : _("TB");
		
	    case 5:
		if (sloppy)
		    /* Peta abbreviated */
		    return classic ? "P" : _("P");
		else
		    /* PetaByte abbreviated */
		    return classic ? "PB" : _("PB");
	}
	
	return string("error");
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
	
	s << f << ' ' << getSuffix(i, classic);
	
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
	    for (int j = 0; j < (classic ? 1 : 2); j++)
	    {
		string suffix = getSuffix(i, classic, j != 0);
		if (boost::iends_with(str_trimmed, suffix, loc))
		{
		    string number = str_trimmed.substr(0, str_trimmed.size() - suffix.size());
		    
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
