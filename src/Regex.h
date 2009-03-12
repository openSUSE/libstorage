#ifndef REGEX_H
#define REGEX_H


/*
 *  Author: Arvin Schnell
 */


#include <regex.h>
#include <string>
#include <boost/noncopyable.hpp>


namespace storage
{
    using std::string;


class Regex : boost::noncopyable
{
public:

    Regex (const char* pattern, int cflags = REG_EXTENDED, unsigned int = 10);
    Regex (const string& pattern, int cflags = REG_EXTENDED, unsigned int = 10);
    ~Regex ();

    string getPattern () const { return pattern; };
    int getCflags () const { return cflags; }

    bool match (const string&, int eflags = 0) const;

    regoff_t so (unsigned int) const;
    regoff_t eo (unsigned int) const;

    string cap (unsigned int) const;

    static const string ws;
    static const string number;

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
