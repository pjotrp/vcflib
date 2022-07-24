/*
    vcflib C++ library for parsing and manipulating VCF files

    Copyright © 2010-2020 Erik Garrison
    Copyright © 2020-2022 Pjotr Prins

    This software is published under the MIT License. See the LICENSE file.
*/

#include "Variant.h"
extern "C" {
#include "vcf-c-api.h"
}
#include <set>
#include <sstream>
#include <getopt.h>

using namespace std;
using namespace vcflib;

// extern "C" void *zig_create_multi_allelic(Variant *retvar, Variant *varlist[], long size);

void printSummary(char** argv) {
    cerr << R"(
Usage: vcfcreatemulti [options] [file]

Go through sorted VCF and if overlapping alleles are represented
across multiple records, merge them into a single record.  Currently
only for indels. See also vcfposmerge for a new implementation.

Type: transformation
)";
    exit(1);
}

Variant createMultiallelic(vector<Variant>& vars) {

    if (vars.size() == 1) {
        return vars.front();
    }

    Variant first = vars.front();
    Variant nvar = first;

    Variant *data = vars.data();
    void *test = &data;
    printf("p:%p\n",&data);
    printf("p:%p\n",data);
    printf("&0:%p\n",&data[0]);
    printf("0:%p\n",data[0]);
    printf("1:%p\n",data[1]);
    printf("&1:%p\n",&data[1]);
    printf("2:%p\n",data[2]);
    printf("t:%p\n",test);
    void **t2 = (void**)test;
    printf("t2:%p\n",t2);
    printf("*t2:%p\n",*t2);
    printf("0:%p\n",t2[0]);
    printf("1:%p\n",t2[1]);
    printf("2:%p\n",t2[2]);
    // Variant *ret = (Variant *)zig_create_multi_allelic(&nvar, t2 , vars.size());

    int i = 0;
    vector<void *> ptrs;
    for (auto v: vars) {
        void *p = &data[i];
        ptrs.push_back(p);
        i++;
    }
    Variant *ret = (Variant *)zig_create_multi_allelic(&nvar, ptrs.data() , vars.size());

    // set maxpos to the most outward allele position + its reference size
    auto maxpos = first.position + first.ref.size();
    for (auto v: vars) {
        if (maxpos < v.position + v.ref.size()) {
            maxpos = v.position + v.ref.size();
        }
    }

    int numalt = vars.size();

    // get REF
    // use start position to extend all other alleles
    int start = first.position;
    string ref = first.ref;

    for (vector<Variant>::iterator v = vars.begin() + 1; v != vars.end(); ++v) {
        int sdiff = (v->position + v->ref.size()) - (start + ref.size());
        int pdiff = (start + ref.size()) - v->position;
        if (sdiff > 0) {
            ref.append(v->ref.substr(pdiff, sdiff));
        }
    }

    Variant mvar = first;
    mvar.alt.clear();
    mvar.ref = ref;

    for (vector<Variant>::iterator v = vars.begin(); v != vars.end(); ++v) {
        // add alternates and splice them into the reference
        int p5diff = v->position - mvar.position;
        int p3diff = (mvar.position + mvar.ref.size()) - (v->position + v->ref.size());
        string before;
        string after;
        if (p5diff > 0) {
            before = mvar.ref.substr(0, p5diff);
        }
        if (p3diff > 0 && p3diff < mvar.ref.size()) {
            after = mvar.ref.substr(mvar.ref.size() - p3diff);
        }
        if (p5diff || p3diff) {
            for (vector<string>::iterator a = v->alt.begin(); a != v->alt.end(); ++a) {
                mvar.alt.push_back(before);
                string& alt = mvar.alt.back();
                alt.append(*a);
                alt.append(after);
            }
        } else {
            for (vector<string>::iterator a = v->alt.begin(); a != v->alt.end(); ++a) {
                mvar.alt.push_back(*a);
            }
        }
    }

    stringstream s;
    s << vars.front().position << "-" << vars.back().position;
    mvar.info["combined"].push_back(s.str());

    return mvar;
}

int main(int argc, char** argv) {

    VariantCallFile variantFile;

    int c;
    while (true) {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            //{"verbose", no_argument,       &verbose_flag, 1},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;

        c = getopt_long (argc, argv, "h",
                         long_options, &option_index);

        if (c == -1)
            break;

        switch (c) {

            case 'h':
                printSummary(argv);
                break;

            case '?':
                printSummary(argv);
                exit(1);
                break;

            default:
                abort ();
        }
    }

    if (optind < argc) {
        string filename = argv[optind];
        variantFile.open(filename);
    } else {
        variantFile.open(std::cin);
    }

    if (!variantFile.is_open()) {
        return 1;
    }

    variantFile.addHeaderLine("##INFO=<ID=combined,Number=1,Type=String,Description=\"Range of overlapping variants which were combined into this one using vcfcreatemulti.\">");

    cout << variantFile.header << endl;

    Variant var(variantFile);
    string lastSeqName;
    vector<Variant> vars;

    // cout << "Calling into Zig" << endl;
    auto var_window = zig_variant_window();

    while (variantFile.getNextVariant(var)) {

        if (lastSeqName.empty()) { // track the previous sequence name
            lastSeqName = var.sequenceName;
        }

        if (vars.empty()) { // track list of variants in window (alt alleles)
            vars.push_back(var);
            Variant *copy2 = &vars.back();
            win_push(var_window,copy2);
            continue;
        } else {
            // compute maxpos as the most right position in the current reference window. Note
            // that the window may get expanded at every step.
            auto first = vars.front();
            auto maxpos = first.position + first.ref.size();
            for (auto v: vars) {
                if (maxpos < v.position + v.ref.size()) {
                    maxpos = v.position + v.ref.size();
                }
            }
            if (var.sequenceName != lastSeqName) {
                // next chromosome, contig or whatever
                Variant result = createMultiallelic(vars);
                cout << result << endl;
                vars.clear();
                lastSeqName = var.sequenceName;
                vars.push_back(var);
                Variant *copy2 = &vars.back();
                win_push(var_window,copy2);
            } else if (var.position < maxpos) {
                // As long as it is in window add it to the list
                vars.push_back(var);
                Variant *copy2 = &vars.back();
                win_push(var_window,copy2);
            } else {
                // Next variant is out of window, so create single line variant
                Variant result = createMultiallelic(vars);
                cout << result << endl;
                vars.clear();
                vars.push_back(var);
                Variant *copy2 = &vars.back();
                win_push(var_window,copy2);
            }
        }
    }

    if (!vars.empty()) {
        Variant result = createMultiallelic(vars);
        cout << result << endl;
    }
    zig_variant_window_cleanup(var_window);

    return 0;

}
