// internal-used helper functions are placed in namespace xylib::util  
// Licence: Lesser GNU Public License 2.1 (LGPL) 
// $Id: util.cpp $

#include "util.h"
#include "xylib.h"

#include <cmath>
#include <climits>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <boost/detail/endian.hpp>
#include <boost/cstdint.hpp>

#if !defined(BOOST_LITTLE_ENDIAN) && !defined(BOOST_BIG_ENDIAN)
#error "Unknown endianess"
#endif

using namespace std;
using namespace xylib::util;
using namespace boost;

namespace xylib {
namespace util {


// -------   standard library functions with added error checking    --------

long my_strtol(const std::string &str) 
{
    string ss = str_trim(str);
    const char *startptr = ss.c_str();
    char *endptr = NULL;
    long val = strtol(startptr, &endptr, 10);

    if (LONG_MAX == val || LONG_MIN == val) {
        throw XY_Error("overflow when reading long");
    } else if (startptr == endptr) {
        throw XY_Error("not an integer as expected");
    }

    return val;
}

double my_strtod(const std::string &str) 
{
    const char *startptr = str.c_str();
    char *endptr = NULL;
    double val = strtod(startptr, &endptr);

    if (HUGE_VAL == val || -HUGE_VAL == val) {
        throw XY_Error("overflow when reading double");
    } else if (startptr == endptr) {
        throw XY_Error("not a double as expected");
    }

    return val;
}


void my_assert(bool condition, const string &msg)
{
    if (!condition) {
        throw XY_Error(msg);
    }
}

// ----------   istream::read()- & endiannes-related utilities   ----------

void my_read(istream &f, char *buf, int len)
{
    f.read(buf, len);
    if (f.gcount() < len) {
        throw XY_Error("unexpected eof");
    }
}

// change the byte-order from "little endian" to host endian
// ptr: pointer to the data, size - size in bytes
#if defined(BOOST_BIG_ENDIAN)
void le_to_host(void *ptr, int size)
{
    char *p = ptr;
    for (int i = 0; i < size/2; ++i)
        swap(p[i], p[size-i-1]);
}
#else
void le_to_host(void *, int) {}
#endif

// read a 32-bit, little-endian form int from f,
// return equivalent int in host endianess
unsigned read_uint32_le(istream &f)
{
    uint32_t val;
    my_read(f, reinterpret_cast<char*>(&val), sizeof(val));
    le_to_host(&val, sizeof(val));
    return val;
}

unsigned read_uint16_le(istream &f)
{
    uint16_t val;
    my_read(f, reinterpret_cast<char*>(&val), sizeof(val));
    le_to_host(&val, sizeof(val));
    return val;
}

int read_int16_le(istream &f)
{
    int16_t val;
    my_read(f, reinterpret_cast<char*>(&val), sizeof(val));
    le_to_host(&val, sizeof(val));
    return val;
}

// the same as above, but read a float
float read_flt_le(istream &f)
{
    float val;
    my_read(f, reinterpret_cast<char*>(&val), sizeof(val));
    le_to_host(&val, sizeof(val));
    return val;
}

// the same as above, but read a float
double read_dbl_le(istream &f)
{
    double val;
    my_read(f, reinterpret_cast<char*>(&val), sizeof(val));
    le_to_host(&val, sizeof(val));
    return val;
}

char read_char(istream &f) 
{
    char val;
    my_read(f, &val, sizeof(val));
    return val;
}

// read a string from f
string read_string(istream &f, unsigned len) 
{
    static char buf[256];
    assert(len < sizeof(buf));
    my_read(f, buf, len);
    buf[len] = '\0';
    return string(buf);
}

//    ----------       string utilities       -----------

// Return a copy of the string str with leading and trailing whitespace removed
string str_trim(string const& str)
{
    std::string ws = " \r\n\t";
    string::size_type first, last;
    first = str.find_first_not_of(ws);
    last = str.find_last_not_of(ws);
    if (first == string::npos) 
        return "";
    return str.substr(first, last - first + 1);
}


// skip whitespace, get key and val that are separated by `sep'
void str_split(string const& line, string const& sep, 
               string &key, string &val)
{
    string::size_type p = line.find_first_of(sep);
    if (p == string::npos) {
        key = line;
        val = "";
    }
    else {
        key = str_trim(line.substr(0, p));
        val = str_trim(line.substr(p + sep.size()));
    }
}

// true if str starts with ss
bool str_startwith(const string &str, const string &ss)
{
    return str.size() >= ss.size() && ss == str.substr(0, ss.size());
}

// change all letters in a string to lower case
std::string str_tolower(const std::string &str)
{
    string r(str);
    for (size_t i = 0; i != str.size(); ++i)
        r[i] = tolower(str[i]);
    return r;
}


//      --------   line-oriented file reading functions   --------

// read a line and return it as a string
string read_line(istream& is) 
{
    string line;
    if (!getline(is, line)) 
        throw xylib::XY_Error("unexpected end of file");
    return line;
}

// skip "count" lines in f
void skip_lines(istream &f, int count)
{
    string line;
    for (int i = 0; i < count; ++i) {
        if (!getline(f, line)) {
            throw XY_Error("unexpected end of file");
        }
    }
}



// get all numbers in the first legal line
void VecColumn::add_values_from_str(string const& str, char sep) 
{
    const char* p = str.c_str();
    while (isspace(*p) || *p == sep)
        ++p;
    while (*p != 0) {
        char *endptr = NULL;
        errno = 0; // To distinguish success/failure after call 
        double val = strtod(p, &endptr);
        if (p == endptr)
            throw("Number not found in line:\n" + str);
        if (errno != 0)
            throw("Numeric overflow or underflow in line:\n" + str);
        add_val(val);
        p = endptr;
        while (isspace(*p) || *p == sep)
            ++p;
    }
}


//TODO: clean this function
// get a line that is not empty and not a comment
bool get_valid_line(std::istream &is, std::string &line, char comment_char)
{
    // read until get a valid line
    while (getline (is, line)) {
        line = str_trim(line);
        if (!line.empty() && line[0] != comment_char) 
            break;
    }

    // trim the "single-line" comments at the tail of a valid line, if any
    string::size_type pos = line.find_first_of(comment_char);
    if (pos != string::npos) {
        line = str_trim(line.substr(0, pos));
    }

    return !is.eof();
}


} // end of namespace util
} // end of namespace xylib

