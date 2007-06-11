// XYlib library is a xy data reading library, aiming to read variaty of xy 
// data formats.
// Licence: GNU General Public License version 2
// $Id: xylib.h $

#include "xylib.h"
#include "common.h"
#include <algorithm>
//#include <boost/regex.hpp>

using namespace std;

namespace xylib
{

XYlib::XYlib(void)
{
}

XYlib::~XYlib(void)
{
}

//////////////////////////////////////////////////////////////////////////
// some functions are borrowed from fityk v0.8.1 code with necessary modifications
void XYlib::load_file(std::string const& file, XY_Data& data, std::string const& type /* =  */)
{
    ifstream f (file.c_str(), ios::in | ios::binary);
    if (!f) {
        throw XY_Error("Can't open file: " + file);
    }

    string const& ft = type.empty() ? guess_ftype(file, f) // "detect" format
        : type;

    data.clear(); //removing previous file
    
    vector<int> col;
    col.push_back(1);
    col.push_back(2);

    if (ft == "text")                         // x y x y ... 
        load_xy_file(f, data, col);
    else if (ft == "uxd")
        load_uxd_file(f, data, col);
    else if (ft == "diffracat_v2_raw")
        load_diffracat_v2_raw_file(f, data);
    else {                                  // other ?
        throw XY_Error("Unknown filetype.");
    }
}

void XYlib::load_xy_file (ifstream& f, XY_Data& data, vector<int> const& columns)
{
    /* format  x y \n x y \n x y \n ...
    *           38.834110      361
    *           38.872800  ,   318
    *           38.911500      352.431
    * delimiters: white spaces and  , : ;
    */
    if (columns.size() == 1 || columns.size() > 3) //0, 2, or 3 columns allowed
        throw XY_Error("If columns are specified, two or three of them"
        " must be given (eg. 1,2,3)");
    vector<int> const& cols = columns;
    vector<fp> xy;
    data.has_sigma = (columns.size() == 3);
    int maxc = *max_element (cols.begin(), cols.end());
    int minc = *min_element (cols.begin(), cols.end());
    if (minc < 0) {
        throw XY_Error("Invalid (negative) column number: " + S(minc));
    }

    //TODO: optimize loop below
    //most of time (?) is spent in getline() in read_line_and_get_all_numbers()
    while (read_line_and_get_all_numbers(f, xy)) {
        if (xy.empty()) {
            continue;
        }

        if (size(xy) < maxc) {
            continue;
        }

        // TODO: need modify, (use col[] specified columns)
        // so far, it is assumed that col 1,2,3 are x, y, sigma
        fp x = xy[cols[0] - 1];
        fp y = xy[cols[1] - 1];
        if (cols.size() == 2)
            data.push_point(XY_Point(x, y));
        else if (cols.size() == 3){
            fp sig = xy[cols[2] - 1];
            if (sig > 0) 
                data.push_point(XY_Point(x, y, sig));
        }
    }
}


void XYlib::load_uxd_file(ifstream& f, XY_Data& data, vector<int> const& columns)
{
    /* format example
    ; C:\httpd\HtDocs\course\content\week-1\sample5.raw - (Diffrac AT V1) File converted by XCH V1.0
    _FILEVERSION=1
    _SAMPLE='D2'.
    ...
    ; (Data for Range number 1)
    _DRIVE='COUPLED'
    _STEPTIME=39.000000
    _STEPSIZE=0.020000
    _STEPMODE='C'
    _START=4.990000
    _2THETA=4.990000
    _THETA=0.000000
    _KHI=0.000000
    _PHI=0.000000
    _COUNTS
    6234      6185      5969      6129      6199      5988      6046      5922
    ...
    */
    data.clear();
/*
    string data_flg = "_COUNTS";
    int data_flg_l = data_flg.size();
*/

    fp x_start, x_step;

    string line;
    while (getline(f, line))
    {
        if (line.empty() || line[0] == ';' || line.find_first_not_of(" \t\r\n") == string::npos)
            continue;     // skip meaningless lines 
        if (line.substr(0, 7) == "_START=") 
        {
            istringstream(line.substr(7)) >> x_start;
            continue;
        }
        if (line.substr(0, 10) == "_STEPSIZE=")
        {
            istringstream(line.substr(10)) >> x_step;
            continue;
        }

        if (line.substr(0, 7) == "_COUNTS")
        {
            int i = 0;
            while (getline(f, line))
            {
                fp val;
                istringstream iss(line);
                while (iss >> val)
                {
                    data.push_point(XY_Point(x_start + i * x_step, val));
                    ++i;
                }
            }
        }
    }
}


void XYlib::load_diffracat_v2_raw_file(std::ifstream& f, XY_Data& data, 
                                unsigned range /*= 0*/)
{
    // TODO: check whether this is Diffract-At v2/3 format (move the verify code to another funciton)
    char buf[256];

    f.clear();

    // get file header
    f.seekg(0);
    f.read(buf, 4);

    if (f.rdstate() & ios::failbit) 
    {
        throw XY_Error("error when reading filehead");
    }

    if (0 != strncmp(buf, "RAW2", 4))
    {
        throw XY_Error("file is not Diffrac-at v2/v3 file");
    }

    // get number of ranges 
    unsigned short ranges = 0;
    f.seekg(4);
    f.read(reinterpret_cast<char*>(&ranges), sizeof(ranges));
    if (range >= ranges)
    {
        sprintf(buf, "file does not have range %d", range);
        throw XY_Error(string(buf));
    }

    // get header of current range
    unsigned short cur_range_start = 256;
    unsigned short cur_header_len = 0;

    // skip the previous data ranges
    for (unsigned cur_range = 0; cur_range < range; ++cur_range)
    {
        f.seekg(cur_range_start);
        f.read(reinterpret_cast<char*>(&cur_header_len), sizeof(cur_header_len));
        cur_range_start += cur_header_len;
    }
    
    // read the specified range data
    //////////////////////////////////////////////////////////////////////////
    unsigned short step_count;
    float step_size, start_x;

    // range header length
    f.seekg(cur_range_start);
    f.read(reinterpret_cast<char*>(&cur_header_len), sizeof(cur_header_len));

    // step count
    f.seekg(cur_range_start + 2);
    f.read(reinterpret_cast<char*>(&step_count), sizeof(step_count));
    
    // step size
    f.seekg(cur_range_start + 12);
    f.read(reinterpret_cast<char*>(&step_size), sizeof(step_size));

    // according to data format specification, here these 24B is of FORTRAN type "6R4"
    // starting axes position for range
    // I find ff[0] is the x coordinate of starting point, and don't konw what others mean
    float ff[6];
    for (int i=0; i<6; ++i)
    {
        f.seekg(cur_range_start + 16 + i * 4);
        f.read(reinterpret_cast<char*>(&ff[i]), sizeof(ff[i]));
    }
    start_x = ff[0];

    // get the x-y data points
    float x, y;
    for (int i=0; i<step_count; ++i)
    {
        f.seekg(cur_range_start + cur_header_len + i * sizeof(y));
        f.read(reinterpret_cast<char*>(&y), sizeof(y));
        x = start_x + i * step_size;
        data.push_point(XY_Point(x, y));
    }
}


// helper functions
//////////////////////////////////////////////////////////////////////////

// guess an unknown file format type. may be expanded by deciding the file 
// type according to the first lines of file
string XYlib::guess_ftype(std::string const fname, std::istream & is,
                          XY_FileInfo* pfi/* = NULL */)
{
    //return "text";
    string::size_type pos = fname.find_last_of('.');

    if (string::npos == pos)
    {
        return "unknown";
    }

    string ext = fname.substr(pos + 1);
    //transform(ext.begin(), ext.end(), ext.begin(), tolower);
    for (string::iterator it = ext.begin(); it != ext.end(); ++it)
    {
        *it = tolower(*it);
    }


    if ("txt" == ext)
    {
        if (pfi)
        {
            pfi->ext_name = ext;
            pfi->type_desc = "text";
            pfi->version = "";
        }
        return "text";
    }
    else if ("uxd" == ext)
    {
        if (pfi)
        {
            pfi->ext_name = ext;
            pfi->type_desc = "SIEMENS/BRUKER ASCII UXD";
            pfi->version = "";
        }
        return "uxd";
    }
    else
    {
        return "unknown";
    }
}


// TODO: this function can be rewritten with boost::regex if necessary
int XYlib::read_line_and_get_all_numbers(istream &is, vector<fp>& result_numbers)
{
    // returns number of numbers in line
    result_numbers.clear();
    string s;
    while (getline(is, s) && 
        (s.empty() || s[0] == '#' 
        || s.find_first_not_of(" \t\r\n") == string::npos))
        ;//ignore lines with '#' at first column or empty lines
    for (string::iterator i = s.begin(); i != s.end(); i++) {
        if (*i == ',' || *i == ';' || *i == ':') {
            *i = ' ';
        }
    }

    istringstream q(s);
    fp d;
    while (q >> d) {
        result_numbers.push_back(d);
    }
    return !is.eof();
}

}


// set XY_FileInfo structrue
void xylib::XYlib::set_file_info(const std::string& ftype, XY_FileInfo* pfi)
{
    if (NULL == pfi)
    {
        return;
    }

    if ("text" == ftype) {
        pfi->ext_name;
        // to be completed, or try to change the interface to do such things?
    }
}
