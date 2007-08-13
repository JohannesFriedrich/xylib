// XYlib library is a xy data reading library, aiming to read variaty of xy 
// data formats.
// Licence: GNU General Public License version 2
// $Id: xylib.cpp $

#include "xylib.h"
#include "util.h"

#include "ds_brucker_raw_v1.h"
#include "ds_brucker_raw_v23.h"
#include "ds_rigaku_dat.h"
#include "ds_text.h"
#include "ds_uxd.h"
#include "ds_vamas.h"
#include "ds_philips_udf.h"
#include "ds_winspec_spe.h"
#include "ds_pdcif.h"

#include <iostream>
#include <algorithm>
#include <boost/detail/endian.hpp>
#include <boost/cstdint.hpp>

using namespace std;
using namespace xylib::util;
using namespace boost;

namespace xylib {

// NOTE: elements in g_fi[] are ordered, 
// they must be kept as the same order as enum xy_ftype
const FormatInfo *g_fi[] = {
    NULL,
    &TextDataSet::fmt_info,
    &UxdDataSet::fmt_info,
    &RigakuDataSet::fmt_info,
    &BruckerV1RawDataSet::fmt_info,
    &BruckerV23RawDataSet::fmt_info,
    &VamasDataSet::fmt_info,
    &UdfDataSet::fmt_info,
    &WinspecSpeDataSet::fmt_info,
    &PdCifDataSet::fmt_info,
};


bool FormatInfo::has_extension(const std::string &ext)
{ 
    string lower_ext = str_tolower(ext);
    return (find(exts.begin(), exts.end(), lower_ext) != exts.end()); 
}

 
//////////////////////////////////////////////////////////////////////////
// member functions of Class Range

void Range::check_idx(unsigned n, const string &name) const
{
    if (n >= get_pt_count()) {
        throw XY_Error("no " + name + " in this range with such index");
    }
}

double Range::get_x(unsigned n) const
{
    check_idx(n, "point_x");
    return x[n];
}

double Range::get_y(unsigned n) const
{
    check_idx(n, "point_y");
    return y[n];
}

bool Range::has_y_stddev(unsigned n) const
{ 
    if (n >= get_pt_count()) {
        return false;
    } else {
        return y_has_stddev[n]; 
    }    
} 

double Range::get_y_stddev(unsigned n) const
{
    check_idx(n, "point_y_stddev");
    return y_stddev[n];
}

void Range::add_pt(double x_, double y_, double stddev_)
{
    x.push_back(x_);
    y.push_back(y_);
    y_stddev.push_back(stddev_);
    y_has_stddev.push_back(true);
}

void Range::add_pt(double x_, double y_)
{
    x.push_back(x_);
    y.push_back(y_);
    y_stddev.push_back(1);  // meaningless data
    y_has_stddev.push_back(false);
}

void Range::export_xy_file(const string &fname) const
{
    ofstream of(fname.c_str());
    if(!of) {
        throw XY_Error("can't create file " + fname + " to output");
    }

    export_xy_file(of);
}


void Range::export_xy_file(ofstream &of) const
{
    int n = get_pt_count();

    for(int i = 0; i < n; ++i) {
        of << setfill(' ') << setiosflags(ios::fixed) << setprecision(5) << setw(7) << 
            get_x(i) << "\t" << setprecision(8) << setw(10) << get_y(i) << "\t";
        if(has_y_stddev(i)) {
            of << get_y_stddev(i);
        }
        of << endl;
    }
}

bool Range::add_meta(const string &key, const string &val)
{
    if ("" == key) {
        return false;
    }
    pair<map<string, string>::iterator, bool> ret = meta_map.insert(make_pair(key, val));
    return ret.second;
}


// return a string vector containing all of the meta-info keys
vector<string> Range::get_all_meta_keys() const
{
    vector<string> keys;

    map<string, string>::const_iterator it = meta_map.begin();
    for (it = meta_map.begin(); it != meta_map.end(); ++it) {
        keys.push_back(it->first);
    }

    return keys;
}

bool Range::has_meta_key(const string &key) const
{
    map<string, string>::const_iterator it = meta_map.find(key);
    return (it != meta_map.end());
}

const string& Range::get_meta(string const& key) const
{
    map<string, string>::const_iterator it;
    it = meta_map.find(key);
    if (meta_map.end() == it) {
        // not found
        throw XY_Error("no such key in meta-info found");
    } else {
        return it->second;
    }
}

//////////////////////////////////////////////////////////////////////////
// member functions of Class FixedStepRange

void FixedStepRange::add_y(double y_)
{
    y.push_back(y_);
    y_stddev.push_back(1);
    y_has_stddev.push_back(false);
}


void FixedStepRange::add_y(double y_, double stddev_)
{
    y.push_back(y_);
    y_stddev.push_back(stddev_);
    y_has_stddev.push_back(true);
}

//////////////////////////////////////////////////////////////////////////
// member functions of Class DataSet

const Range& DataSet::get_range(unsigned i) const
{
    if (i >= ranges.size()) {
        throw XY_Error("no range in this file with such index");
    }

    return *(ranges[i]);
}


void DataSet::export_xy_file(const string &fname, 
    bool with_meta /* = true */, const std::string &cmt_str /* = ";" */) const
{
    unsigned range_num = get_range_cnt();
    ofstream of(fname.c_str());
    if (!of) {
        throw XY_Error("can't create file" + fname);
    }

    // output the file-level meta-info
    if (with_meta) {
        of << cmt_str << "exported by xylib from a " << get_filetype() << "file" << endl;
        output_meta(of, this, cmt_str);
        of << cmt_str << "total ranges:" << ranges.size() << endl;
    }
    
    for(unsigned i = 0; i < range_num; ++i) {
        const Range &rg = get_range(i);
        if (with_meta) {
            of << endl;
            for (int j = 0; j < 40; ++j) {
                of << cmt_str;
            }
            of << endl << cmt_str << "* range " << i << endl;
            output_meta(of, &rg, cmt_str);
            of << cmt_str << "total count: " << rg.get_pt_count() << endl << endl;
            of << cmt_str << "x\t\ty\t\t y_stddev" << endl;
        }
        rg.export_xy_file(of);
    }
}

bool DataSet::add_meta(const string &key, const string &val)
{
    if ("" == key) {
        return false;
    }
    
    pair<map<string, string>::iterator, bool> ret = meta_map.insert(make_pair(key, val));
    return ret.second;
}


// return a string vector containing all of the meta-info keys
vector<string> DataSet::get_all_meta_keys() const
{
    vector<string> keys;

    map<string, string>::const_iterator it = meta_map.begin();
    for (it = meta_map.begin(); it != meta_map.end(); ++it) {
        keys.push_back(it->first);
    }

    return keys;
}

bool DataSet::has_meta_key(const string &key) const
{
    map<string, string>::const_iterator it = meta_map.find(key);
    return (it != meta_map.end());
}

const string& DataSet::get_meta(string const& key) const
{
    map<string, string>::const_iterator it;
    it = meta_map.find(key);
    if (meta_map.end() == it) {
        // not found
        throw XY_Error("no such key in meta-info found");
    } else {
        return it->second;
    }
}

DataSet::~DataSet()
{
    vector<Range*>::iterator it;
    for (it = ranges.begin(); it != ranges.end(); ++it) {      
        delete *it;
    }
}

//////////////////////////////////////////////////////////////////////////
// members of UxdLikeDataSet

line_type UxdLikeDataSet::get_line_type(const string &line)
{
    string str = str_trim(line);
   
    if (str.empty()) {
        return LT_EMPTY;
    } else {
        char ch = str[0];
        if (str_startwith(str, cmt_start)) {
            return LT_COMMENT;
        } else if (string::npos != str.find_first_of(meta_sep)) {
            return LT_KEYVALUE;
        } else if (isdigit(ch) || ch == '+' || ch == '-') {
            return LT_XYDATA;
        } else {
            return LT_UNKNOWN;
        }
    }    
}


// move the reading ptr of "f" to a line which has meaning to us
// return ture if not eof, false otherwise
bool UxdLikeDataSet::skip_invalid_lines(std::istream &f)
{
    string line;
    if (!peek_line(f, line, false)) {
        f.seekg(-1, ios_base::end); // set eof
        return false;
    }
    
    line_type type = get_line_type(line);
    while (LT_COMMENT == type || LT_EMPTY == type) {
        skip_lines(f, 1);
        if (!peek_line(f, line, false)) {
            f.seekg(-1, ios_base::end); // set eof
            return false;
        }
        type = get_line_type(line);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
// namespace scope global functions
DataSet* getNewDataSet(istream &is, xy_ftype filetype /* = FT_UNKNOWN */, 
    const string &filename /* = "" */)
{
    DataSet *pd = NULL;
    xy_ftype ft = (FT_UNKNOWN == filetype) ? guess_file_type(filename) : filetype;

    if (FT_BR_RAW1 == ft) {
        pd = new BruckerV1RawDataSet(is); 
    } else if (FT_BR_RAW23 == ft) {
        pd = new BruckerV23RawDataSet(is);
    } else if (FT_UXD == ft) {
        pd = new UxdDataSet(is);
    } else if (FT_TEXT == ft) {
        pd = new TextDataSet(is);
    } else if (FT_RIGAKU == ft) {
        pd = new RigakuDataSet(is);
    } else if (FT_VAMAS == ft) {
        pd = new VamasDataSet(is);
    } else if (FT_UDF == ft) {
        pd = new UdfDataSet(is);
    } else if (FT_SPE == ft) {
        pd = new WinspecSpeDataSet(is);
    } else if (FT_PDCIF == ft) {
        pd = new PdCifDataSet(is);
    } else {
        pd = NULL;
        throw XY_Error("unkown or unsupported file type");
    }

    if (NULL != pd) {
        pd->load_data(); 
    }

    return pd;
}


xy_ftype guess_file_type(const string &fpath)
{
    string::size_type pos = fpath.find_last_of('.');

    if(string::npos == pos) {
        return FT_UNKNOWN;
    }

    string ext = str_tolower(fpath.substr(pos + 1));
    ifstream f(fpath.c_str(), ios::in | ios::binary);

    try {
        if("txt" == ext || "asc" == ext) {
            return FT_TEXT;
        } else if ("uxd" == ext) {
            return FT_UXD;
        } else if ("vms" == ext) {
            return FT_VAMAS;
        } else if ("udf" == ext) {
            return FT_UDF;
        } else if ("raw" == ext) {
            // may be brucker_raw_v1 or v2/v3. notice the order
            if (BruckerV23RawDataSet::check(f)) {
                return FT_BR_RAW23;
            } else {
                return FT_BR_RAW1;
            }
        } else if ("dat" == ext) {
            // may be "text" or "rigaku"
            if (RigakuDataSet::check(f)) {
                return FT_RIGAKU;
            } else {
                return FT_TEXT;
            }
        } else if ("spe" == ext) {
            return FT_SPE;
        } else if ("cif" == ext) {
            return FT_PDCIF;
        } else {
            return FT_UNKNOWN;
        }
    } catch (const runtime_error &e) {
        cout << "excption in guess_file_type(" << fpath << ")" << endl;
        cout << "excption: " << e.what() << endl;
        return FT_UNKNOWN;
    }
}


xy_ftype string_to_ftype(const std::string &ftype_name) {
    for (int i = 0; i < FT_NUM; ++i) {
        if (g_fi[i] && ftype_name == g_fi[i]->name) {
            return static_cast<xy_ftype>(i);
        }
    }
    return FT_UNKNOWN;
}


} // end of namespace xylib


