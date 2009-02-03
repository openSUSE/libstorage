#ifndef HUMAN_STRING_H
#define HUMAN_STRING_H


#include <string>


namespace storage
{

    /**
     * Return a pretty description of a size with required precision
     * and using B, kB, MB, GB, TB or PB as unit as appropriate.
     *
     * @param size size in bytes
     * @param classic use classic locale
     * @param precision number of fraction digits in output
     * @param omit_zeroes if true omit trailing zeroes for exact values
     * @return formatted string
     */
    std::string byteToHumanString(unsigned long long size, bool classic, int precision,
				  bool omit_zeroes);

    /**
     * Converts a size description using B, kB, MB, GB, TB or PB into an integer.
     *
     * @param str size string
     * @param classic use classic locale
     * @param size size in bytes
     * @return true on successful conversion
     *
     * The conversion is always case-insensitive. With classic set to
     * false the conversion is also sloppy concerning omission of 'B'.
     */
    bool humanStringToByte(const std::string& str, bool classic, unsigned long long& size);

}


#endif
