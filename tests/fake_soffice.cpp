// SPDX-License-Identifier: Apache-2.0
//
// CX-03 test seam: a standalone stand-in for LibreOffice's soffice.exe.
// TestConversionExtraction copies this executable to
// <appDir>/libreoffice/program/soffice.exe — ConversionManager::locateSoffice
// checks the bundled location FIRST — so convertOfficeToPdf runs this instead
// of a real LibreOffice. The production code invokes it with the fixed argv
//
//   fake_soffice [--env:UserInstallation=<uri>] --headless
//                --convert-to pdf:writer_pdf_Export --outdir <dir> <input>
//
// so the TEST controls the outcome through the environment:
//   GLYPHPDF_FAKE_SOFFICE_MODE=noop    exit 0, write NOTHING (converter
//                                      "succeeds" without a product — the
//                                      CX-03 data-loss trigger);
//   GLYPHPDF_FAKE_SOFFICE_MODE=copy    copy the file named by
//                                      GLYPHPDF_FAKE_SOFFICE_PRODUCT to
//                                      <outdir>/<input-basename>.pdf (a real,
//                                      loadable product);
//   GLYPHPDF_FAKE_SOFFICE_MODE=garbage write "%PDF-" + junk (starts like a
//                                      PDF, must be refused by loadability);
//   anything else                      exit 0, write nothing (defaults to the
//                                      noop shape).
// GLYPHPDF_FAKE_SOFFICE_EXIT=1 forces a nonzero exit (converter failure).
//
// Deliberately no Qt: a plain C++ translation unit keeps the helper a few KB
// and immune to test-framework churn.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

static void copyFile(const std::string& from, const std::string& to)
{
    FILE* in = std::fopen(from.c_str(), "rb");
    if (!in) return;
    FILE* out = std::fopen(to.c_str(), "wb");
    if (!out) { std::fclose(in); return; }
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), in)) > 0)
        std::fwrite(buf, 1, n, out);
    std::fclose(in);
    std::fclose(out);
}

int main(int argc, char** argv)
{
    std::string outDir;
    std::string inputPath;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--outdir" && i + 1 < argc) outDir = argv[++i];
        else if (!a.empty() && a[0] != '-') inputPath = a;  // the office file
    }

    const char* exitEnv = std::getenv("GLYPHPDF_FAKE_SOFFICE_EXIT");
    const int forcedExit = exitEnv ? std::atoi(exitEnv) : 0;

    const char* modeEnv = std::getenv("GLYPHPDF_FAKE_SOFFICE_MODE");
    const std::string mode = modeEnv ? modeEnv : "";

    if (inputPath.empty() || outDir.empty()) return forcedExit;

    // <input-basename>.pdf — LibreOffice's own naming rule.
    std::string base = inputPath;
    const size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    const std::string product = outDir + "/" + base + ".pdf";

    if (mode == "copy") {
        const char* prodEnv = std::getenv("GLYPHPDF_FAKE_SOFFICE_PRODUCT");
        if (prodEnv) copyFile(prodEnv, product);
    } else if (mode == "garbage") {
        FILE* f = std::fopen(product.c_str(), "wb");
        if (f) {
            std::fwrite("%PDF-1.4 this is not a pdf\n", 1, 27, f);
            std::fclose(f);
        }
    } // noop: write nothing

#ifdef _WIN32
    if (forcedExit != 0) ExitProcess(static_cast<UINT>(forcedExit));
#endif
    return forcedExit;
}
