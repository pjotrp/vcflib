/*
    vcflib C++ library for parsing and manipulating VCF files

    Copyright © 2010-2020 Erik Garrison
    Copyright © 2020      Pjotr Prins

    This software is published under the MIT License. See the LICENSE file.
*/

#include "convert.h"

#include "Variant.h"

using namespace std;
using namespace vcflib;

int main(int argc, char** argv) {

    VariantCallFile variantFile;

    if (argc == 1) {
      cerr << "usage: " << argv[0] << " [key] [vcf_file]" << endl << endl
             << "Puts QUAL into an info field tag keyed by [key]." << endl
           << endl;
      cerr << endl << "Type: transformation" << endl << endl;

        return 1;
    }

    string key = argv[1];

    if (argc > 2) {
        string filename = argv[2];
        variantFile.open(filename);
    } else {
        variantFile.open(std::cin);
    }

    if (!variantFile.is_open()) {
        return 1;
    }

    variantFile.addHeaderLine("##INFO=<ID="+key+",Number=1,Type=Float,Description=\"QUAL value of site field.\">");

    cout << variantFile.header << endl;

    Variant var(variantFile);
    while (variantFile.getNextVariant(var)) {
        var.info[key].clear();
        var.info[key].push_back(convert(var.quality));
        cout << var << endl;
    }

    return 0;

}
