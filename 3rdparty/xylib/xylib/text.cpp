// ascii plain text Format
// Licence: Lesser GNU Public License 2.1 (LGPL) 
// $Id$

/*

FORMAT DESCRIPTION:
====================

X-Y plain text format.

///////////////////////////////////////////////////////////////////////////////
    * Name in progam:   text
    * Extension name:   txt, dat, asc, prn
    * Binary/Text:      text
    * Multi-blocks:     N
    
///////////////////////////////////////////////////////////////////////////////
    * Format details: 
In every valid date line, the format is 
x y [std_dev]
x y [std_dev]
...
delimiters between X and Y may be white spaces or  , : ;
There may be some comment lines without any valid XY data

///////////////////////////////////////////////////////////////////////////////
    * Sample Fragment: ("#xxx": comments added by me; ...: omitted lines)

; Sample date: 2000/12/31 21:32        # comments
38.834110      361
38.872800  ,   318                     # delimiters may be "," etc.
38.911500      352.431
    
///////////////////////////////////////////////////////////////////////////////
    * Implementation Ref of xylib: based on the analysis of the sample files.

*/

#include "text.h"
#include "rigaku_dat.h"
#include "util.h"

using namespace std;
using namespace xylib::util;

namespace xylib {

const static string exts[5] = { "txt", "dat", "asc", "csv", "prn" };

const FormatInfo TextDataSet::fmt_info(
    "text",
    "ascii plain text",
    vector<string>(exts, exts + sizeof(exts) / sizeof(string)),
    false,                       // whether binary
    false,                       // whether has multi-blocks
    &TextDataSet::ctor,
    &TextDataSet::check
);

bool TextDataSet::check(istream & /*f*/) 
{
    return true;
}

// skip meaningless lines and get all numbers in the first legal line
int read_line_and_get_all_numbers(istream &is, 
    vector<double>& result_numbers)
{
    // returns number of numbers in line
    result_numbers.clear();
    string s;
    while (getline(is, s) && 
        (s.empty() || s[0] == '#' 
        || s.find_first_not_of(" \t\r\n") == string::npos))
        ;   // ignore lines with '#' at first column or empty lines
    for (string::iterator i = s.begin(); i != s.end(); i++) {
        if (*i == ',' || *i == ';' || *i == ':') {
            *i = ' ';
        }
    }

    istringstream q(s);
    double d;
    while (q >> d) {
        result_numbers.push_back(d);
    }
    return !is.eof();
}

void TextDataSet::load_data(std::istream &f) 
{
    Block* p_blk = new Block;
    vector<VecColumn*> vec_cols;  
    unsigned nr_cols(0);
    vector<double> xy;
    while (read_line_and_get_all_numbers(f, xy)) {
        if (xy.empty()) {
            continue;
        }

        my_assert(xy.size() != 1, "only one number in a line");

        if (0 == nr_cols) {
            // for the 1st row of numbers
            nr_cols = xy.size();
            for (unsigned i = 0; i < nr_cols; ++i) {
                VecColumn *p_col = new VecColumn;
                vec_cols.push_back(p_col);
                p_blk->add_column(p_col);
            }
        } else {
            my_assert(xy.size() == nr_cols, "format error: number of columns differ");
        }

        for (unsigned i = 0; i < nr_cols; ++i) {
            vec_cols[i]->add_val(xy[i]);
        }
    }

    blocks.push_back(p_blk);
}

} // end of namespace xylib

