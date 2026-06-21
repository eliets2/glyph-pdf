#include <QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/VeraPdfValidator.h"

// TestVeraPdf — veraPDF CLI subprocess integration tests.
//
// veraPDF is located at runtime (VeraPdfValidator::locateCli): a bundled copy,
// the GLYPHPDF_VERAPDF env var, or the PATH. When none is present, isAvailable()
// returns false and initTestCase() calls QSKIP, so the suite is counted as
// skipped (not failed) by ctest — the normal CI path when veraPDF isn't installed.
// When a validator is found, the test functions exercise the real subprocess.

using namespace gp;

class TestVeraPdf : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString& name) {
        return m_tmpDir.filePath(name);
    }

    // Create a minimal plain PDF (not PDF/A) using PoDoFo.
    // A plain PDF lacks the XMP metadata, OutputIntent, and other structural
    // requirements for PDF/A-2b, so veraPDF should report violations.
    QString createPlainPdf() {
        QString path = tmpPath("plain.pdf");
        PoDoFo::PdfMemDocument doc;
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        doc.Save(path.toUtf8().constData());
        return path;
    }

private slots:
    void initTestCase() {
        if (!VeraPdfValidator::isAvailable()) {
            QSKIP("veraPDF not found at runtime (bundle/env/PATH) — skipping integration tests");
        }
        QVERIFY(m_tmpDir.isValid());
    }

    // isAvailable() must be true when we reach here (initTestCase would have skipped otherwise)
    void testValidatorAvailabilityReflected() {
        QVERIFY(VeraPdfValidator::isAvailable());
    }

    // A plain PDF (no PDF/A metadata) must fail PDF/A-2b validation and
    // report at least one violation.
    void testMalformedPdfReportsViolations() {
        QString path = createPlainPdf();
        auto report = VeraPdfValidator::validate(path, PdfAConformance::PDF_A_2B);

        QVERIFY2(report.validatorAvailable,
            "validatorAvailable should be true when CLI is present");
        QVERIFY2(!report.isValid,
            "Plain PDF should not conform to PDF/A-2b");
        QVERIFY2(!report.violations.isEmpty(),
            "At least one violation should be reported for a plain PDF");

        // Each returned violation must have a non-empty ruleId and severity
        for (const RuleViolation& v : report.violations) {
            QVERIFY2(!v.ruleId.isEmpty(), "ruleId must not be empty");
            QCOMPARE(v.severity, QString("error"));
        }
    }

    // Passing a nonexistent file path should not crash — the validator is
    // available (CLI found) but the output will either be an error message or
    // a failed validation with no violations.
    void testNonExistentFileDoesNotCrash() {
        auto report = VeraPdfValidator::validate(
            tmpPath("does_not_exist_12345.pdf"),
            PdfAConformance::PDF_A_2B);

        QVERIFY(report.validatorAvailable);
        // isValid is false when file not found; errorMessage may or may not be set
        // The critical requirement: no exception, no crash
        QVERIFY(!report.isValid || !report.errorMessage.isEmpty() || !report.violations.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestVeraPdf)
#include "TestVeraPdf.moc"
