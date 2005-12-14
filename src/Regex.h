#ifndef REGEX_H
#define REGEX_H


/*
 *  Author: Arvin Schnell
 */


#include <sys/types.h>
#include <regex.h>
#include <string>

using std::string;

namespace storage
{

class Regex
{
public:

    Regex (const char*, int = REG_EXTENDED, unsigned int = 10);
    Regex (const string&, int = REG_EXTENDED, unsigned int = 10);
    ~Regex ();

    string getPattern () const { return pattern; };
    int getCflags () const { return cflags; }

    bool match (const string&, int = 0) const;

    regoff_t so (unsigned int) const;
    regoff_t eo (unsigned int) const;

    string cap (unsigned int) const;
    static const string& ws;
    static const string& number;

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
