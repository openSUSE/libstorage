#ifndef REGEX_H
#define REGEX_H


/*
 *  Author: Arvin Schnell
 */


#include <sys/types.h>
#include <regex.h>
#include <string>

using std::string;

class Regex
{
public:

    Regex (const char*, int = REG_EXTENDED, unsigned int = 10);
    Regex (const string&, int = REG_EXTENDED, unsigned int = 10);
    ~Regex ();

    string get_pattern () const { return pattern; };
    int get_cflags () const { return cflags; }

    bool match (const string&, int = 0) const;

    regoff_t so (unsigned int) const;
    regoff_t eo (unsigned int) const;

    string cap (unsigned int) const;

private:
    const string pattern;
    const int cflags;
    const unsigned int nm;

    mutable regex_t rx;
    mutable int my_nl_msg_cat_cntr;
    mutable regmatch_t* rm;

    mutable string last_str;
};

#endif
