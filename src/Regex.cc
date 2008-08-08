/*
 *  Author: Arvin Schnell
  Textdomain    "storage"
 */


#include "Regex.h"

using namespace storage;


extern int _nl_msg_cat_cntr;


Regex::Regex (const char* pattern, int cflags, unsigned int nm)
    : pattern (pattern),
      cflags (cflags),
      nm (cflags & REG_NOSUB ? 0 : nm)
{
    regcomp (&rx, pattern, cflags);
    my_nl_msg_cat_cntr = _nl_msg_cat_cntr;
    rm = new regmatch_t[nm];
}


Regex::Regex (const string& pattern, int cflags, unsigned int nm)
    : pattern (pattern),
      cflags (cflags),
      nm (cflags & REG_NOSUB ? 0 : nm)
{
    regcomp (&rx, pattern.c_str (), cflags);
    my_nl_msg_cat_cntr = _nl_msg_cat_cntr;
    rm = new regmatch_t[nm];
}


Regex::~Regex ()
{
    delete [] rm;
    regfree (&rx);
}


bool
Regex::match (const string& str, int eflags) const
{
    if (my_nl_msg_cat_cntr != _nl_msg_cat_cntr) {
	regfree (&rx);
	regcomp (&rx, pattern.c_str (), cflags);
	my_nl_msg_cat_cntr = _nl_msg_cat_cntr;
    }

    last_str = str;

    return regexec (&rx, str.c_str (), nm, rm, eflags) == 0;
}


regoff_t
Regex::so (unsigned int i) const
{
    return i < nm ? rm[i].rm_so : -1;
}


regoff_t
Regex::eo (unsigned int i) const
{
    return i < nm ? rm[i].rm_eo : -1;
}


string
Regex::cap (unsigned int i) const
{
    if (i < nm && rm[i].rm_so > -1)
	return last_str.substr (rm[i].rm_so, rm[i].rm_eo - rm[i].rm_so);
    return "";
}

const string& Regex::ws = "[ \t]*";
const string& Regex::number = "[0123456789]+";
