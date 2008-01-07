// Licence: Lesser GNU Public License 2.1 (LGPL) 
// $Id$

// Siemens/Bruker Diffrac-AT Raw Format version 1/2/3
//  - data format used in Siemens/Brucker X-ray diffractometers.
// Based on the file format specification:
// "Appendix B: DIFFRAC-AT Raw Data File Format" from a diffractometer manual 

#include "brucker_raw.h"
#include "util.h"

using namespace std;
using namespace xylib::util;

namespace xylib {

const FormatInfo BruckerRawDataSet::fmt_info(
    "bruker_raw",
    "Siemens/Bruker RAW ver. 1/2/3",
    vector<string>(1, "raw"),
    true,                       // whether binary
    true,                       // whether has multi-blocks
    &BruckerRawDataSet::ctor,
    &BruckerRawDataSet::check
);


bool BruckerRawDataSet::check(istream &f)
{
    string head = read_string(f, 4);
    return head == "RAW " || head == "RAW2";
}


void BruckerRawDataSet::load_data(std::istream &f) 
{
    string head = read_string(f, 4);
    format_assert(head == "RAW " || head == "RAW2"); 
    if (head[3] == ' ')
        load_version1(f);
    else
        load_version2(f);
}

void BruckerRawDataSet::load_version1(std::istream &f) 
{
    unsigned following_range = 1;
    
    while (following_range > 0) {
        Block* p_blk = new Block;
    
        unsigned cur_range_steps = read_uint32_le(f);  
        // early DIFFRAC-AT raw data files didn't repeat the "RAW " 
        // on additional ranges
        // (and if it's the first block, 4 bytes from file were already read)
        if (!blocks.empty()) {
            char rawstr[4] = {'R', 'A', 'W', ' '};
            le_to_host(rawstr, 4);
            if (cur_range_steps == *reinterpret_cast<uint32_t*>(rawstr))
                cur_range_steps = read_uint32_le(f);
        }
        
        p_blk->meta["MEASUREMENT_TIME_PER_STEP"] = S(read_flt_le(f));
        float x_step = read_flt_le(f); 
        p_blk->meta["SCAN_MODE"] = S(read_uint32_le(f));
        f.ignore(4); 
        float x_start = read_flt_le(f);

        StepColumn *p_xcol = new StepColumn(x_start, x_step);
        
        float t = read_flt_le(f);
        if (-1e6 != t)
            p_blk->meta["THETA_START"] = S(t);
            
        t = read_flt_le(f);
        if (-1e6 != t)
            p_blk->meta["KHI_START"] = S(t);
            
        t = read_flt_le(f);
        if (-1e6 != t)
            p_blk->meta["PHI_START"], S(t);

        p_blk->meta["SAMPLE_NAME"] = read_string(f, 32);
        p_blk->meta["K_ALPHA1"] = S(read_flt_le(f));
        p_blk->meta["K_ALPHA2"] = S(read_flt_le(f));

        f.ignore(72);   // unused fields
        following_range = read_uint32_le(f);
        
        VecColumn *p_ycol = new VecColumn();
        
        for(unsigned i = 0; i < cur_range_steps; ++i) {
            float y = read_flt_le(f);
            p_ycol->add_val(y);
        }
        p_blk->set_xy_columns(p_xcol, p_ycol);

        blocks.push_back(p_blk);
    } 
}

void BruckerRawDataSet::load_version2(std::istream &f) 
{
    unsigned range_cnt = read_uint16_le(f);

    // add file-scope meta-info
    f.ignore(162);
    meta["DATE_TIME_MEASURE"] = read_string(f, 20);
    meta["CEMICAL SYMBOL FOR TUBE ANODE"] = read_string(f, 2);
    meta["LAMDA1"] = S(read_flt_le(f));
    meta["LAMDA2"] = S(read_flt_le(f));
    meta["INTENSITY_RATIO"] = S(read_flt_le(f));
    f.ignore(8);
    meta["TOTAL_SAMPLE_RUNTIME_IN_SEC"] = S(read_flt_le(f));

    f.ignore(42);   // move ptr to the start of 1st block
    for (unsigned cur_range = 0; cur_range < range_cnt; ++cur_range) {
        Block* p_blk = new Block;

        // add the block-scope meta-info
        unsigned cur_header_len = read_uint16_le(f);
        my_assert (cur_header_len > 48, "block header too short");

        unsigned cur_range_steps = read_uint16_le(f);
        f.ignore(4);
        p_blk->meta["SEC_PER_STEP"] = S(read_flt_le(f));
        
        float x_step = read_flt_le(f);
        float x_start = read_flt_le(f);
        StepColumn *p_xcol = new StepColumn(x_start, x_step);

        f.ignore(26);
        p_blk->meta["TEMP_IN_K"] = S(read_uint16_le(f));

        f.ignore(cur_header_len - 48);  // move ptr to the data_start
        VecColumn *p_ycol = new VecColumn;
        
        for(unsigned i = 0; i < cur_range_steps; ++i) {
            float y = read_flt_le(f);
            p_ycol->add_val(y);
        }
        p_blk->set_xy_columns(p_xcol, p_ycol);
        blocks.push_back(p_blk);
    }
}

} // end of namespace xylib

