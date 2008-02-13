// Powder Diffraction CIF (pdCIF)
// Licence: Lesser GNU Public License 2.1 (LGPL) 
// $Id$


#include <map>

//#define BOOST_SPIRIT_DEBUG
#include <boost/spirit/core.hpp>
#include <boost/spirit/utility/confix.hpp>
#include <boost/spirit/utility/chset.hpp>

#include "pdcif.h"
#include "util.h"

using namespace std;
using namespace boost::spirit;
using namespace xylib::util;

namespace xylib {

const FormatInfo PdCifDataSet::fmt_info(
    "pdcif",
    "The Crystallographic Information File for Powder Diffraction",
    vector<string>(1, "cif"),
    false,                      // not binary
    true,                       // multi-blocks
    &PdCifDataSet::ctor,
    &PdCifDataSet::check
);

bool PdCifDataSet::check(istream &f) 
{
    string line;
    // the 1st line (that is not a comment) must start with "data_"
    if (!get_valid_line(f, line, '#') || !str_startwith(line, "data_")) 
        return false;

    // in pdCIF, there must be at least a tag whose name starts with "_pd_"
    while (get_valid_line(f, line, '#')) 
        if (str_startwith(line, "_pd_")) 
            return true;

    return false;
}

namespace {

// Based on: http://www.iucr.org/iucr-top/cif/spec/version1.1/cifsyntax.html
// Appendix A: A formal grammar for CIF
//
// there are errors in this spec:
// - no whitespace between LoopHeader and LoopBody
// - extra "|" in <TokenizedComments> (...<eol> |}...)
// - there is <AnyPrintChar> in <SingleQuotedString> and <DoubleQuotedString>
//   and single-quote(') and double-quote(") are also in AnyPrintChar
// - reserved words are not excluded from UnquotedString
//
// Known issues:
// - to make is simpler, UnquotedString can't start with semicolon,
//   and SemiColonTextField don't have to start after EOL. 

// types of <Value> 
const int v_inapplicable = 0;
const int v_unknown = 1;
const int v_numeric = 2;
const int v_numeric_with_err = 3;
const int v_text = 4;

template <typename Actions>
struct CifGrammar : public grammar<CifGrammar<Actions> >
{
    CifGrammar(Actions& actions_)
        : actions(actions_) {}

    template <typename ScannerT>
    struct definition
    {
        definition(CifGrammar const& self)
        {
            // CIF 'syntactic units' that have equivalent Spirit parsers:
            //            CIF            |        Spirit
            //      ---------------------+---------------------------
            //     <Digit>               |        digit_p
            //     <UnsignedInteger>     |        uint_p
            //     <Integer>             |        int_p
            //     <Float>               |        real_p
            //     DATA_                 |        as_lower_d["data_"]
            //
            //     (LOOP_, GLOBAL_, SAVE_, STOP_ are similar to DATA_)

            Actions& actions = self.actions;

            chset<> OrdinaryChar("a-zA-Z0-9!%&()*+,./:<=>?@\\`^{|}~-");
            chset<> NonBlankChar("\"#$'_;[]" 
                                 "a-zA-Z0-9!%&()*+,./:<=>?@\\`^{|}~-");
            chset<> AnyPrintChar("\"#$'_;[] \t" 
                                 "a-zA-Z0-9!%&()*+,./:<=>?@\\`^{|}~-");
            //<TextLeadChar> is AnyPrintChar - ";"

            BOOST_SPIRIT_DEBUG_NODE(CIF);
            BOOST_SPIRIT_DEBUG_NODE(DataBlock);
            BOOST_SPIRIT_DEBUG_NODE(DataBlockHeading);
            BOOST_SPIRIT_DEBUG_NODE(SaveFrame);
            BOOST_SPIRIT_DEBUG_NODE(SaveFrameHeading);
            BOOST_SPIRIT_DEBUG_NODE(DataItems);
            BOOST_SPIRIT_DEBUG_NODE(LoopHeader);
            BOOST_SPIRIT_DEBUG_NODE(LoopBody);
            BOOST_SPIRIT_DEBUG_NODE(Tag);
            BOOST_SPIRIT_DEBUG_NODE(Value);
            BOOST_SPIRIT_DEBUG_NODE(CharString);
            BOOST_SPIRIT_DEBUG_NODE(TextField);
            BOOST_SPIRIT_DEBUG_NODE(WhiteSpace);
            BOOST_SPIRIT_DEBUG_NODE(Comments);
            BOOST_SPIRIT_DEBUG_NODE(TokenizedComments);
            BOOST_SPIRIT_DEBUG_NODE(SkipLine);
            BOOST_SPIRIT_DEBUG_NODE(PeekNoReservedWord);

            CIF 
                = !Comments >> !WhiteSpace
                  >> !( DataBlock >> *(WhiteSpace >> DataBlock) >> !WhiteSpace
                      )
                ;

            DataBlock 
                = DataBlockHeading 
                  >> *( WhiteSpace >> (DataItems 
                                   // | SaveFrame 
                                      ) 
                      )
                  >> eps_p [actions.on_block_finish]
                ;

            DataBlockHeading 
                = as_lower_d["data_"] 
                  >> (+NonBlankChar) [actions.on_block_start] 
                ;

            // Save frames may only be used in dictionary files, 
            // so we don't care about it
            
            //SaveFrame 
            //    = SaveFrameHeading 
            //      >> +( WhiteSpace >> DataItems )
            //      >> WhiteSpace >> as_lower_d["save_"]
            //    ;

            //SaveFrameHeading 
            //    = as_lower_d["save_"] >> +NonBlankChar
            //    ;

            DataItems 
                = (Tag >> WhiteSpace >> Value) [actions.on_tag_value_finish]
                | (LoopHeader >> LoopBody) [actions.on_loop_finish]
                | SkipLine // ignore invalid lines
                ;

            // There is a lot of invalid CIF files in the world. It's not 
            // very suprising - if the spec contains so many errors
            // who would bother to implement it properly. It's not clear
            // to me if there is a kind of informal spec all CIF tools 
            // conform to. 
            // The most common bug is a missing or non-standard value in 
            // tag-value pair.
            SkipLine 
                = PeekNoReservedWord >> +AnyPrintChar 
                ;


            LoopHeader 
                = as_lower_d["loop_"] 
                  >> + ( WhiteSpace >> Tag ) [actions.on_loop_tag] 
                ;

            LoopBody
                //= + ( WhiteSpace >> Value ) [actions.on_loop_value]
                // we allow loop without values - to parse invalid files
                = * ( WhiteSpace >> Value ) [actions.on_loop_value]
                ;

            Tag 
                = '_' >> (+NonBlankChar) [assign_a(actions.last_tag)]
                ;

            // The special values of '.' and '?' represent data that are 
            // inapplicable or unknown, respectively.
            Value
                = (real_p  [assign_a(actions.value_real)]
                   >> ( ( '(' >> uint_p  >> ch_p(')') 
                           [assign_a(actions.last_value, v_numeric_with_err)] 
                        )
                      | eps_p [assign_a(actions.last_value, v_numeric)]
                      ) 
                  ) [assign_a(actions.value_string)]
                  >> eps_p(space_p)
                | ch_p('.')  [assign_a(actions.last_value, v_unknown)]
                | ch_p('?')  [assign_a(actions.last_value, v_inapplicable)]
                | CharString [assign_a(actions.last_value, v_text)]
                | TextField  [assign_a(actions.last_value, v_text)]
                ;

            PeekNoReservedWord
                = ~eps_p(as_lower_d[str_p("data_") | "loop_" | "global_" 
                                   | "save_" | "stop_"])
                ;


            CharString
                  // UnquotedString (we don't check for preceding ';')
                = PeekNoReservedWord
                  >> (OrdinaryChar >> *NonBlankChar) 
                                              [assign_a(actions.value_string)]
                  // SingleQuotedString
                | ch_p('\'') 
                  >> ( *chset<>(AnyPrintChar - '\'') ) 
                                              [assign_a(actions.value_string)]
                  >> '\'' >> eps_p(space_p)
                  // DoubleQuotedString
                | ch_p('"') 
                  >> ( *chset<>(AnyPrintChar - '"') )
                                              [assign_a(actions.value_string)]
                  >> ch_p('"') >> eps_p(space_p)
                ;

            TextField
                = ch_p(';') 
                  >> ( *AnyPrintChar>> +eol_p
                       >>   *( chset<>(AnyPrintChar - ';') >> *AnyPrintChar 
                               >> +eol_p)
                     ) [assign_a(actions.value_string)]
                  >> ';'; 
                ;

            WhiteSpace
                = +(+space_p >> !Comments)
                ;
            Comments
                = + ('#' >> *AnyPrintChar >> eol_p)
                ;
        }

        rule<ScannerT> CIF, DataBlock, DataBlockHeading, SaveFrame, 
                       SaveFrameHeading, DataItems, LoopHeader, LoopBody,
                       Tag, Value,
                       CharString, TextField,
                       WhiteSpace, TokenizedComments, Comments,
                       PeekNoReservedWord, SkipLine;

        rule<ScannerT> const& start() const { return CIF; }
    };

    Actions& actions;
};

class DatasetActions;

//#define DECLARE_DATASET_ACTION(name) \
//
struct t_on_block_start {
    DatasetActions &da;
    t_on_block_start(DatasetActions &da_) : da(da_) {}
    template <typename IteratorT>
    void operator()(IteratorT, IteratorT b) const;
};

struct t_on_block_finish {
    DatasetActions &da;
    t_on_block_finish(DatasetActions &da_) : da(da_) {}
    template <typename IteratorT>
    void operator()(IteratorT, IteratorT) const;
};

struct t_on_tag_value_finish {
    DatasetActions &da;
    t_on_tag_value_finish(DatasetActions &da_) : da(da_) {}
    template <typename IteratorT>
    void operator()(IteratorT, IteratorT) const;
};

struct t_on_loop_tag {
    DatasetActions &da;
    t_on_loop_tag(DatasetActions &da_) : da(da_) {}
    template <typename IteratorT>
    void operator()(IteratorT, IteratorT) const;
};

struct t_on_loop_value {
    DatasetActions &da;
    t_on_loop_value(DatasetActions &da_) : da(da_) {}
    template <typename IteratorT>
    void operator()(IteratorT, IteratorT) const;
};

struct t_on_loop_finish {
    DatasetActions &da;
    t_on_loop_finish(DatasetActions &da_) : da(da_) {}
    template <typename IteratorT>
    void operator()(IteratorT, IteratorT) const;
};

struct LoopValue
{
    double val;
    double err;
    LoopValue() : val(0.), err(-1) {}
    LoopValue(double val_) : val(val_), err(0.) {}
    LoopValue(string const& s) 
    {
        string::size_type lb_pos = s.find('(');
        string val_s = s.substr(0, lb_pos);
        string err_s = s.substr(lb_pos + 1, s.size() - lb_pos - 2);
        //TODO  ;
    } 
};

struct DatasetActions
{
    string last_tag;
    int last_value;
    double value_real;
    string value_string;
    vector<string> loop_tags;
    vector<LoopValue> loop_values;

    t_on_block_start on_block_start;
    t_on_block_finish on_block_finish;
    t_on_tag_value_finish on_tag_value_finish;
    t_on_loop_tag on_loop_tag;
    t_on_loop_value on_loop_value;
    t_on_loop_finish on_loop_finish;

    Block *block;
    vector<Block*> block_list;

    DatasetActions() 
        : on_block_start(*this),
          on_block_finish(*this),
          on_tag_value_finish(*this),
          on_loop_tag(*this),
          on_loop_value(*this),
          on_loop_finish(*this),
          block(NULL) {}
};

template <typename IteratorT>
void t_on_block_start::operator()(IteratorT a, IteratorT b) const 
{
    assert(da.block == NULL);
    da.block = new Block;
    da.block->name = string(a, b);
}

template <typename IteratorT>
void t_on_block_finish::operator()(IteratorT a, IteratorT b) const 
{
    assert(da.block != NULL);
    da.block_list.push_back(da.block);
    da.block = NULL;
}

template <typename IteratorT>
void t_on_tag_value_finish::operator()(IteratorT, IteratorT) const 
{
    string s;
    if (da.last_value == v_numeric) 
        s = S(da.value_real);
    else if (da.last_value == v_numeric_with_err)
        s = da.value_string;
    else if (da.last_value == v_text)
        s = da.value_string;
    else
        // if it is inapplicable and unknown value, we do nothing
        return;
    da.block->meta[da.last_tag] = s;
}

template <typename IteratorT>
void t_on_loop_tag::operator()(IteratorT, IteratorT) const 
{
    da.loop_tags.push_back(da.last_tag);
}

template <typename IteratorT>
void t_on_loop_value::operator()(IteratorT, IteratorT) const 
{
    if (da.last_value == v_numeric) 
        da.loop_values.push_back(LoopValue(da.value_real));
    else if (da.last_value == v_numeric_with_err)
        da.loop_values.push_back(LoopValue(da.value_string));
    else
        da.loop_values.push_back(LoopValue());
}

template <typename IteratorT>
void t_on_loop_finish::operator()(IteratorT, IteratorT) const 
{
    int n = da.loop_values.size() / da.loop_tags.size();
    // make columns
    //TODO
    da.loop_values.clear();
    da.loop_tags.clear();
}

} // anonymous namespace


void PdCifDataSet::load_data(std::istream &f) 
{
    // interesting data - data names without _pd_ prefix
    static const char* valued_keys[] = {
        // x-axis
        //_pd_meas_2theta_range_
        //_pd_proc_2theta_range_
        "pd_meas_2theta_scan",
        "pd_meas_time_of_flight",
        "pd_proc_2theta_corrected",
        "pd_proc_d_spacing",
        "pd_proc_energy_incident",
        "pd_proc_energy_detection",
        "pd_proc_recip_len_Q",
        "pd_proc_wavelength",

        // y-axis
        "pd_meas_counts_total",
        "pd_meas_counts_background",
        "pd_meas_counts_container",
        "pd_meas_intensity_total",
        "pd_meas_intensity_background",
        "pd_meas_intensity_container",
        "pd_proc_intensity_net",
        "pd_proc_intensity_total",
        "pd_proc_intensity_bkg_calc",
        "pd_proc_intensity_bkg_fix",
        "pd_calc_intensity_net",
        "pd_calc_intensity_total",

        // weight, sigma, etc.
        "pd_meas_step_count_time",
        "pd_meas_counts_monitor",
        "pd_meas_intensity_monitor",
        "pd_proc_intensity_norm",
        "pd_proc_intensity_incident",
        "pd_proc_ls_weight",
        NULL
    };

    // read file into vector<char>
    f.unsetf(ios::skipws); 
    vector<char> vec;
    std::copy(istream_iterator<char>(f), istream_iterator<char>(),
              std::back_inserter(vec));
    DatasetActions dataset_actions;
    CifGrammar<DatasetActions> p(dataset_actions);
    parse_info<vector<char>::const_iterator> info =
        parse(vec.begin(), vec.end(), p);
    format_assert(info.full,
                  "Parse error at character " + S(info.stop - vec.begin()));



    Block* blk = new Block;
    for (int i = 0; i < 2; ++i) {
        VecColumn *col = new VecColumn();
        blk->add_column(col);
    }
    blocks.push_back(blk);
}


} // namespace xylib

