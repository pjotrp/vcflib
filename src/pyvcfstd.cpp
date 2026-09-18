/*
    pyvcfstd - pybind11 bindings for vcf-std, the VCFv4.5 field
    validation library. Used by the doctests in test/pytest/vcf-std.md.

    Every validator returns None when the value is valid and raises
    ValueError with the descriptive spec message otherwise.
*/

#include <pybind11/pybind11.h>

#include <string>

extern "C" {
#include "vcf-std.h"
}

namespace py = pybind11;

static void raise_on_error(int rc, const vcfstd_error &err) {
    if (rc != VCFSTD_OK) {
        throw py::value_error(std::string(err.msg));
    }
}

#define WRAP(name, fn) \
    m.def(#name, [](const std::string &s) { \
        vcfstd_error err; \
        raise_on_error(fn(s.c_str(), &err), err); \
    }, py::arg("value"), "Validate a " #name " field; raises ValueError on invalid input");

PYBIND11_MODULE(pyvcfstd, m) {
    m.doc() = "VCFv4.5 field validation (plain C core, see src/vcf-std.c)";

    WRAP(check_chrom,  vcfstd_check_chrom)
    WRAP(check_pos,    vcfstd_check_pos)
    WRAP(check_id,     vcfstd_check_id)
    WRAP(check_ref,    vcfstd_check_ref)
    WRAP(check_alt,    vcfstd_check_alt)
    WRAP(check_qual,   vcfstd_check_qual)
    WRAP(check_filter, vcfstd_check_filter)
    WRAP(check_info,   vcfstd_check_info)
    WRAP(check_format, vcfstd_check_format)

    m.def("check_gt", [](const std::string &gt, int nalt) {
        vcfstd_error err;
        raise_on_error(vcfstd_check_gt(gt.c_str(), nalt, &err), err);
    }, py::arg("gt"), py::arg("nalt"),
       "Validate a GT value against an ALT allele count");

    m.def("check_sample", [](const std::string &format, const std::string &sample) {
        vcfstd_error err;
        raise_on_error(vcfstd_check_sample(format.c_str(), sample.c_str(), &err), err);
    }, py::arg("format"), py::arg("sample"),
       "Validate a sample column against a FORMAT column");

    m.def("count_alt", [](const std::string &alt) {
        int n = vcfstd_count_alt(alt.c_str());
        if (n == -1) throw py::value_error("ALT: malformed field");
        return n;
    }, py::arg("alt"), "Number of ALT alleles ('.' counts as 0)");

    m.def("validate_record", [](const std::string &line, bool deep) {
        vcfstd_error err;
        raise_on_error(vcfstd_validate_record_flags(
                           line.c_str(), deep ? VCFSTD_DEEP : VCFSTD_BASIC, &err),
                       err);
    }, py::arg("line"), py::arg("deep") = true,
       "Validate a complete VCF data line; raises ValueError on invalid input. "
       "deep=True (default) also runs the expensive per-sample GT range checks.");
}
