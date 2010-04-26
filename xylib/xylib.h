// Public API of xylib library.
// Licence: Lesser GNU Public License 2.1 (LGPL)
// $Id$

/// xylib is a library for reading files that contain x-y data from powder
/// diffraction, spectroscopy or other experimental methods.
///
/// It is recommended to set LC_NUMERIC="C" (or other locale with the same
/// numeric format) before reading files.
///
/// Usually, we first call load_file() to read file from disk. It stores
/// all data from the file in class DataSet.
/// DataSet contains a list of Blocks, each Blocks contains a list of Columns,
/// and each Column contains a list of values.
///
/// It may sound complex, but IMO it can't be made simpler.
/// It's analogical to a spreadsheet. One OOCalc or Excel file (which
/// corresponds to xylib::DataSet) contains a number of sheets (Blocks),
/// but usually only one is used. We can view each sheet as a list of columns.
///
/// In xylib all columns in one block must have equal length.
/// Several filetypes always contain only one Block with two Columns.
/// In this case we can take coordinates of the 15th point as:
///    double x = get_block(0)->get_column(1)->get_value(14);
///    double y = get_block(0)->get_column(2)->get_value(14);
/// Note that blocks and points are numbered from 0, but columns are numbered
/// from 1, because the column 0 returns index of point.
/// All values are stored as floating-point numbers, even if they are integers
/// in the file.
/// DataSet and Block contain also MetaData, which is a string to string map.
///
/// The API uses std::string. It may not be 

#ifndef XYLIB_XYLIB_H_
#define XYLIB_XYLIB_H_

// XYLIB_API is a mark for API classes and functions,
// used to decorate classes and functions for Win32 DLL linking.
#ifdef XYLIB_API
# undef XYLIB_API
#endif
#if defined(_WIN32) && defined(BUILDING_XYLIB) && \
        (defined(XYLIB_DLL) || defined(DLL_EXPORT))
# define XYLIB_API  __declspec(dllexport)
#elif defined(_WIN32) && defined(XYLIB_DLL)
# define XYLIB_API  __declspec(dllimport)
#else
# define XYLIB_API
#endif

#ifdef __cplusplus

#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <algorithm>

/// Library version. Use get_version() to get it as a string.
///  XYLIB_VERSION % 100 is the sub-minor version
///  XYLIB_VERSION / 100 % 100 is the minor version
///  XYLIB_VERSION / 10000 is the major version

#define XYLIB_VERSION 600 // 0.6.0


namespace xylib
{

/// see also XYLIB_VERSION
XYLIB_API const char* get_version();

class DataSet;

/// stores format related info
struct XYLIB_API FormatInfo
{
    typedef bool (*t_checker)(std::istream&);
    typedef DataSet* (*t_ctor)();

    std::string name;  /// short name, usually basename of .cpp/.h files
    std::string desc;  /// full format name (reasonably short)
    //TODO replace vector with |-separated string
    std::vector<std::string> exts; // possible extensions
    bool binary; /// true if it's binary file
    bool multiblock; /// true if filetype supports multiple blocks

    t_ctor ctor; /// factory function
    t_checker checker; /// function used to check if a file has this format

    FormatInfo(std::string const& name_,
               std::string const& desc_,
               std::vector<std::string> const& exts_,
               bool binary_,
               bool multiblock_,
               t_ctor ctor_,
               t_checker checker_)
        : name(name_), desc(desc_), exts(exts_),
          binary(binary_), multiblock(multiblock_),
          ctor(ctor_), checker(checker_) {}

    /// check if extension `ext' is in the list `exts'; case insensitive
    bool has_extension(std::string const& ext) const;
    /// check if file f can be of this format
    bool check(std::istream& f) const { return !checker || (*checker)(f); }
};

/// all supported filetypes can be iterated by calling this function
/// with 0, 1, ... until NULL is returned.
XYLIB_API const FormatInfo* get_format(int n);


/// unexpected format, unexpected EOF, etc
class XYLIB_API FormatError : public std::runtime_error
{
public:
    FormatError(std::string const& msg) : std::runtime_error(msg) {};
};

/// all errors other than format error
class XYLIB_API RunTimeError : public std::runtime_error
{
public:
    RunTimeError(std::string const& msg) : std::runtime_error(msg) {};
};


/// abstract base class for a column
class XYLIB_API Column
{
public:
    virtual ~Column() {}

    /// Column can have a name (but usually it doesn't have)
    virtual std::string const& get_name() const = 0;

    /// return number of points or -1 for "unlimited" number of points
    virtual int get_point_count() const = 0;

    /// return value of n'th point (starting from 0-th)
    virtual double get_value(int n) const = 0;

    /// get minimum value in column
    virtual double get_min() const = 0;

    /// get maximum value in column;
    /// point_count must be specified if column has "unlimited" length, it is
    /// ignored otherwise
    virtual double get_max(int point_count=0) const = 0;

    /// returns step in the case of fixed step, 0. otherwise
    virtual double get_step() const = 0;
};


struct MetaDataImp; // implementation details
struct BlockImp;

/// Map that stores meta-data (additional data, that usually describe x-y data)
/// for block or dataset. For example: date of the experiment, wavelength, ...
/// The only way to get all elements is using size(), get_key() and get().
class XYLIB_API MetaData
{
public:
    // use these functions to query meta data
    bool has_key(std::string const& key) const;
    std::string const& get(std::string const& key) const;
    size_t size() const;
    std::string const& get_key(size_t index) const;

    // functions for use only in xylib
    MetaData();
    ~MetaData();
    void operator=(const MetaData& other);
    void clear();
    bool set(std::string const& key, std::string const& val);
    std::string& operator[] (const std::string& x);

private:
    MetaData(const MetaData&); // disallow

    MetaDataImp *imp_;
};


/// a block of data
class XYLIB_API Block
{
public:
    /// handy pseudo-column that returns index of point as value
    static Column* const index_column;

    MetaData meta; /// meta-data

    Block();
    ~Block();

    /// block can have a name (but usually it doesn't have)
    std::string const& get_name() const;

    /// number of real columns, not including 0-th pseudo-column
    int get_column_count() const;
    /// get column, 0-th column is index of point
    const Column& get_column(int n) const;

    /// return number of points or -1 for "unlimited" number of points
    /// each column should have the same number of points (or "unlimited"
    /// number if the column is a generator)
    int get_point_count() const;

    // functions for use in filetype implementations
    void add_column(Column* c, bool append=true);
    void del_column(int n);
    void set_name(std::string const& name);

private:
    Block(const Block&); // disallow

    BlockImp* imp_;
};


/// DataSet represents data stored typically in one file.
// may consist of one or more block(s) of X-Y data
class XYLIB_API DataSet
{
public:
    // pointer to FormatInfo of a class derived from DataSet
    FormatInfo const* const fi;

    MetaData meta; /// meta-data

    // ctor is protected
    virtual ~DataSet();

    /// number of blocks (usually 1)
    int get_block_count() const;

    /// get block n (block 0 is first)
    Block const* get_block(int n) const;

    /// read data from file
    virtual void load_data(std::istream &f) = 0;

    /// delete all data stored in this class (use only if you want to
    /// call load_data() more than once)
    void clear();

    /// check if options (first arg) contains given element (second arg)
    bool has_option(std::string const& t);

    // functions for use in filetype implementations
    void add_block(Block* block);
    // if load_data() supports options, set it before it's called
    void set_options(std::vector<std::string> const& options);
    //TODO pass format_name and options as one space-separated string(!)

protected:
    DataSet(FormatInfo const* fi_);

private:
    std::vector<Block*> blocks;

    std::vector<std::string> options_;
};


/// if format_name is not given, it is guessed
/// return value: pointer to Dataset that contains all data read from file
XYLIB_API DataSet* load_file(std::string const& path,
                             std::string const& format_name="",
                             std::vector<std::string> const& options
                                                = std::vector<std::string>());

/// return value: pointer to Dataset that contains all data read from file
XYLIB_API DataSet* load_stream(std::istream &is, FormatInfo const* fi,
                               std::vector<std::string> const& options);

/// guess a format of the file; does NOT handle compressed files
XYLIB_API FormatInfo const* guess_filetype(std::string const& path,
                                           std::istream &f);

/// returns FormatInfo that has a name format_name
XYLIB_API FormatInfo const* string_to_format(std::string const& format_name);

/// return wildcard for file dialog in format:
/// "ASCII X Y Files (*)|*|Sietronics Sieray CPI (*.cpi)|*.cpi"
XYLIB_API std::string get_wildcards_string(std::string const& all_files="*");

} // namespace xylib

extern "C" {

#endif // !__cplusplus

/* Minimal C API.
 * Note that blocks and rows are indexed from 0, but columns are indexed from 1,
 * because pseudo-column 0 contains indices of points.
 **/
XYLIB_API const char* xylib_get_version();
XYLIB_API void* xylib_load_file(const char* path, const char* format_name);
XYLIB_API void* xylib_get_block(void* dataset, int block);
XYLIB_API int xylib_count_columns(void* block);
XYLIB_API int xylib_count_rows(void* block, int column);
XYLIB_API double xylib_get_data(void* block, int column, int row);
XYLIB_API const char* xylib_dataset_metadata(void* dataset, const char* key);
XYLIB_API const char* xylib_block_metadata(void* block, const char* key);
XYLIB_API void xylib_free_dataset(void* dataset);

#ifdef __cplusplus
}
#endif //__cplusplus

// For internal use only.
#ifdef BUILDING_XYLIB
// macro used in declarations of classes derived from DataSet
#define OBLIGATORY_DATASET_MEMBERS(class_name) \
    public: \
        class_name() : DataSet(&fmt_info) {} \
        void load_data(std::istream &f); \
        static bool check(std::istream &f); \
        static DataSet* ctor() { return new class_name; } \
        static const FormatInfo fmt_info;
#endif

#endif // XYLIB_XYLIB_H_

