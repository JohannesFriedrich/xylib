// Header of class UxdDataSet
// Licence: Lesser GNU Public License 2.1 (LGPL) 
// $Id: ds_uxd.h $

#ifndef UXD_DATASET_H
#define UXD_DATASET_H
#include "xylib.h"

namespace xylib {
    class UxdDataSet : public UxdLikeDataSet
    {
    public:
        UxdDataSet()
            : UxdLikeDataSet(FT_UXD) 
        {
            rg_start_tag = "_DRIVE";
            x_start_key = "_START";
            x_step_key = "_STEPSIZE";
            meta_sep = "=";
            data_sep = " ";
            cmt_start = ";";
        }

        // implement the interfaces specified by DataSet
        void load_data(std::istream &f);

        static bool check(std::istream &f);

        const static FormatInfo fmt_info;
        
    protected:
        void parse_range(Range *p_rg);
    }; 
}
#endif // #ifndef UXD_DATASET_H

