// Convert file supported by xylib to ascii format
// Licence: Lesser GNU Public License 2.1 (LGPL) 
// $Id$

#include <iostream>
#include <iomanip>
#include <string>
#include <string.h>

#include "xylib/xylib.h"

using namespace std;
 
void print_usage()
{
    cout << "Usage:\n" 
            "\txyconv [-m] [-t FILETYPE] INPUT_FILE OUTPUT_FILE\n"
            "\txyconv -l\n\n"
            "  Converts INPUT_FILE to ascii OUTPUT_FILE\n"
            "  -l              list all supported file types.\n"
            "  -m              write also meta-info.\n"
            "  -t              specify filetype of input file.\n";
}

void list_supported_formats()
{
    for (int i = 1; i < xylib::FT_NUM; ++i) 
        cout << setw(20) << left << xylib::g_fi[i]->name << ": "
             << xylib::g_fi[i]->desc << endl;
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "-l") == 0) {
        list_supported_formats();
        return 0;
    }

    if (argc == 2 && 
            (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        print_usage();
        return 0;
    }

    if (argc < 3) {
        print_usage();
        return -1;
    }

    bool meta = false;
    string filetype;
    for (int i = 1; i < argc - 2; ++i) {
        if (strcmp(argv[i], "-m") == 0) 
            meta = true;
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc - 2) {
            ++i;
            filetype = argv[i];
        }
        else {
            print_usage();
            return -1;
        }
    }

    ifstream ifs(argv[argc-2], ios::in | ios::binary);
    if (!ifs) {
        cerr << "Error: can't open input file " << argv[argc-2] << endl;
        return -1;
    }

    try {
        xylib::xy_ftype t = filetype.empty() ? xylib::FT_UNKNOWN
                                             : xylib::string_to_ftype(filetype);
        xylib::DataSet *d = xylib::getNewDataSet(ifs, t, argv[argc-2]);
        d->export_xy_file(argv[argc-1], meta, "#");
        delete d;
    } catch (xylib::XY_Error const& e) {
        cerr << "Error: " << e.what() << endl;
        return -1;
    }
    return 0;
}

