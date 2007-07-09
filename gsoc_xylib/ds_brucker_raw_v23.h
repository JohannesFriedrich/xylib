// Header of class BruckerV23RawDataSet for reading meta-data and xy-data from 
// Siemens/Bruker Diffrac-AT Raw File v2/v3 format
// Licence: GNU General Public License version 2
// $Id: __MY_FILE_ID__ $

#ifndef BRUCKER_RAW_V23_H
#define BRUCKER_RAW_V23_H

class BruckerV23RawDataSet : public DataSet
{
public:
    BruckerV23RawDataSet(const std::string &filename)
        : DataSet(filename, FT_BR_RAW23) {}

    // implement the interfaces specified by DataSet
    bool is_filetype() const;
    void load_data();
}; 

#endif // #ifndef BRUCKER_RAW_V23_H

