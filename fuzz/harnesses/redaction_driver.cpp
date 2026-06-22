// fuzz/harnesses/redaction_driver.cpp
//
// HEADLESS redaction driver for GlyphPDF's PoDoFoBackend (via PdfEditorEngine).
// Produces REAL redacted PDFs on disk so the Python oracles in fuzz/oracle/
// can inspect every xref revision and assert redacted content is gone.
//
// APPARATUS, not a weapon: exercises the owner's own redaction feature with
// controlled inputs, writes only into paths passed on the command line
// (scratch). NEVER edits production source.
//
// To avoid coupling the driver to a specific PoDoFo high-level ABI, the SOURCE
// PDFs are emitted as RAW BYTES (we fully control the content stream, including
// the exact "a b c d e f Tm" matrix for the F-02 probe). The driver then feeds
// that file to the REAL engine: loadDocumentForEditing -> applyRedactions ->
// saveDocument (the plain Save path -> F-01).
//
// Modes (argv[1]):
//   simple <out.pdf> <secret> [rx ry rw rh]
//   tm     <out.pdf> <secret> <a> <b> <c> <d> <e> <f> [rx ry rw rh]
//
// Build: see fuzz/build_clang/build_redaction_driver.sh (g++ hand-link recipe)
//        and fuzz/CMakeLists.txt (CMake target reusing pdfws_engines).
//
// Geometry: PDF user space, origin bottom-left. Letter-ish page 612x792.

#include <QCoreApplication>
#include <QRectF>
#include <QList>
#include <QString>
#include <QByteArray>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include "engines/PdfEditorEngine.h"

// ---- raw PDF emission ------------------------------------------------------

static std::string escapePdfLiteral(const std::string& s) {
    std::string o;
    for (char ch : s) {
        if (ch == '(' || ch == ')' || ch == '\\') o.push_back('\\');
        o.push_back(ch);
    }
    return o;
}

// Emit a minimal one-page PDF whose single text run uses the given text matrix.
// For the "simple" case pass identity (1 0 0 1 100 700).
static bool emitRawPdf(const std::string& path, const std::string& secret,
                       double a, double b, double c, double d, double e, double f) {
    std::string lit = escapePdfLiteral(secret);

    std::ostringstream cs;
    cs << "BT\n/F1 18 Tf\n"
       << a << " " << b << " " << c << " " << d << " " << e << " " << f
       << " Tm\n(" << lit << ") Tj\nET\n";
    std::string content = cs.str();

    // Build the body, tracking byte offsets for the xref table.
    std::ostringstream pdf;
    std::vector<size_t> off(6, 0);   // offsets for objects 1..5
    auto mark = [&](int n){ off[n] = pdf.str().size(); };

    pdf << "%PDF-1.7\n%\xE2\xE3\xCF\xD3\n";
    mark(1);
    pdf << "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n";
    mark(2);
    pdf << "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n";
    mark(3);
    pdf << "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
           "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>\nendobj\n";
    mark(4);
    pdf << "4 0 obj\n<< /Length " << content.size() << " >>\nstream\n"
        << content << "endstream\nendobj\n";
    mark(5);
    pdf << "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n";

    size_t xrefOff = pdf.str().size();
    pdf << "xref\n0 6\n";
    pdf << "0000000000 65535 f \n";
    char buf[32];
    for (int i = 1; i <= 5; ++i) {
        std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", off[i]);
        pdf << buf;
    }
    pdf << "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n" << xrefOff << "\n%%EOF\n";

    std::ofstream out(path, std::ios::binary);
    if (!out) { std::fprintf(stderr, "[driver] cannot write %s\n", path.c_str()); return false; }
    out << pdf.str();
    return true;
}

// Drive the REAL engine: load, redact rect, save.
//   incremental==false -> saveDocument()  (plain Save  -> F-01 orphan probe)
//   incremental==true  -> writeUpdate()   (SaveUpdate  -> F-03 revision probe)
static bool driveRedaction(const std::string& inPath, const std::string& outPath,
                           const QRectF& rect, bool incremental,
                           bool* appliedOut, bool* savedOut) {
    PdfEditorEngine engine;
    if (!engine.loadDocumentForEditing(QString::fromUtf8(inPath.c_str()))) {
        std::fprintf(stderr, "[driver] loadDocumentForEditing failed: %s\n", inPath.c_str());
        return false;
    }
    bool ok = engine.applyRedactions(0, QList<QRectF>{rect});
    if (appliedOut) *appliedOut = ok;
    if (!ok) std::fprintf(stderr, "[driver] applyRedactions returned false (saving anyway)\n");
    bool saved = incremental
        ? engine.writeUpdate(QString::fromUtf8(outPath.c_str()))
        : engine.saveDocument(QString::fromUtf8(outPath.c_str()));
    if (savedOut) *savedOut = saved;
    if (!saved) {
        std::fprintf(stderr, "[driver] %s failed: %s\n",
                     incremental ? "writeUpdate" : "saveDocument", outPath.c_str());
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr,
          "usage:\n"
          "  %s simple <out.pdf> <secret> [rx ry rw rh]\n"
          "  %s tm     <out.pdf> <secret> <a> <b> <c> <d> <e> <f> [rx ry rw rh]\n",
          argv[0], argv[0]);
        return 2;
    }
    std::string mode = argv[1];
    std::string outPath = argv[2];
    std::string srcPath = outPath + ".src.pdf";

    // Default rect in QRectF top-left coords. The engine flips to PDF space via
    // (pageHeight - y - h), so y=60,h=70 maps to PDF y 662..732 which covers the
    // identity-Tm baseline at PDF y=700.  (Earlier default y=680 landed at the
    // PAGE BOTTOM and produced a misleading "leak" — fixed.)
    double rx = 60, ry = 60, rw = 400, rh = 70;

    if (mode == "simple" || mode == "incr") {
        bool incremental = (mode == "incr");
        std::string secret = (argc > 3) ? argv[3] : "SECRET_DATA_XYZZY";
        if (argc >= 8) { rx=atof(argv[4]); ry=atof(argv[5]); rw=atof(argv[6]); rh=atof(argv[7]); }
        if (!emitRawPdf(srcPath, secret, 1,0,0,1,100,700)) return 1;
        bool applied=false, saved=false;
        if (!driveRedaction(srcPath, outPath, QRectF(rx,ry,rw,rh), incremental, &applied, &saved))
            return 1;
        std::printf("OK %s src=%s out=%s secret=%s applied=%d saved=%d rect=%g,%g,%g,%g\n",
                    mode.c_str(), srcPath.c_str(), outPath.c_str(), secret.c_str(),
                    applied, saved, rx,ry,rw,rh);
        return 0;
    }
    if (mode == "tm") {
        if (argc < 10) { std::fprintf(stderr, "tm mode needs secret + a b c d e f\n"); return 2; }
        std::string secret = argv[3];
        double a=atof(argv[4]), b=atof(argv[5]), c=atof(argv[6]),
               d=atof(argv[7]), e=atof(argv[8]), ff=atof(argv[9]);
        if (argc >= 14) { rx=atof(argv[10]); ry=atof(argv[11]); rw=atof(argv[12]); rh=atof(argv[13]); }
        if (!emitRawPdf(srcPath, secret, a,b,c,d,e,ff)) return 1;
        bool applied=false, saved=false;
        if (!driveRedaction(srcPath, outPath, QRectF(rx,ry,rw,rh), false, &applied, &saved))
            return 1;
        std::printf("OK tm src=%s out=%s secret=%s applied=%d Tm=%g,%g,%g,%g,%g,%g rect=%g,%g,%g,%g\n",
                    srcPath.c_str(), outPath.c_str(), secret.c_str(), applied,
                    a,b,c,d,e,ff, rx,ry,rw,rh);
        return 0;
    }
    std::fprintf(stderr, "unknown mode: %s (use simple|incr|tm)\n", mode.c_str());
    return 2;
}
