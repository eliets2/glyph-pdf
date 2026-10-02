// SPDX-License-Identifier: Apache-2.0
// Scorecard row 15 (wave 2b): EXPORTABLE accessibility results.
//
// The checker (engines/AccessibilityChecker) produces an honest A11yReport;
// this suite pins the WRITER that turns that report into compliance
// artifacts (engines/A11yReportWriter, modeled on ReviewSummaryWriter):
//   * CSV — machine-readable, the repo's ONE escaping contract
//     (ConversionManager::csvFormulaSafeCell + RFC-4180 quoting, the same
//     discipline as CommentsWidget::csvEscapeField) so formula-injection
//     hardening cannot drift between exporters;
//   * PDF — a print-ready summary through the SafeSave candidate
//     transaction, standard-14 Helvetica with the W1-04 per-string
//     sanitizing (non-WinAnsi content degrades the STRING, never the file);
//   * HONESTY — the artifact carries the checker's verdict, its capability
//     degradations (bounded per-object samples + truncation totals, load
//     failures) and the standing "never certifies PDF/UA" disclosure. A
//     failed scan is exported as failed — never greenwashed into a clean
//     report, and zero findings are never worded as conformance.
//
// Serial offscreen only (SafeSave shared %TEMP%/glyphpdf-candidates seam
// class FU-2, same as TestPrintableSummary/TestAccessibilityFixes).

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include <podofo/podofo.h>

#include "engines/A11yReportWriter.h"
#include "engines/AccessibilityChecker.h"
#include "engines/SafeSave.h"
#include "engines/pdfium/PdfiumBackend.h"

namespace {

gp::A11yFinding makeFinding(gp::A11ySeverity severity, const QString& checkId,
                            const QString& where, const QString& whyNot,
                            int page, const QString& targetId = QString()) {
    gp::A11yFinding f;
    f.severity = severity;
    f.checkId = checkId;
    f.where = where;
    f.whyNot = whyNot;
    f.page = page;
    f.targetId = targetId;
    return f;
}

// A synthetic loaded report: two HIGH, one MEDIUM, one LOW finding plus a
// disclosed truncation (60 of 100 fields sampled) — every degradation the
// checker can carry, in one artifact.
gp::A11yReport loadedReport() {
    gp::A11yReport r;
    r.path = QStringLiteral("contract.pdf");
    r.loadOk = true;
    r.tagged = false;
    r.imagesReported = 1;
    r.imagesTotal = 1;
    r.fieldsReported = 60;
    r.fieldsTotal = 100;
    r.hasDocTitle = false;
    r.findings = {
        makeFinding(gp::A11ySeverity::High, QStringLiteral("struct-tree"),
                    QStringLiteral("document"),
                    QStringLiteral("No structure tree — screen readers see an "
                                   "undifferentiated text stream"), -1),
        makeFinding(gp::A11ySeverity::High, QStringLiteral("image-alt"),
                    QStringLiteral("page 3 · image /Im0"),
                    QStringLiteral("No /Alt description; excluded from "
                                   "assistive technology"), 2,
                    QStringLiteral("Im0")),
        makeFinding(gp::A11ySeverity::Medium, QStringLiteral("doc-language"),
                    QStringLiteral("document"),
                    QStringLiteral("No /Lang — screen readers cannot pick the "
                                   "right pronunciation rules"), -1),
        makeFinding(gp::A11ySeverity::Low, QStringLiteral("display-doc-title"),
                    QStringLiteral("document"),
                    QStringLiteral("Window title does not show the document "
                                   "title"), -1),
    };
    return r;
}

gp::A11yReport cleanReport() {
    gp::A11yReport r;
    r.path = QStringLiteral("clean.pdf");
    r.loadOk = true;
    r.tagged = true;
    r.imagesReported = 0;
    r.imagesTotal = 0;
    r.fieldsReported = 0;
    r.fieldsTotal = 0;
    r.hasDocTitle = true;
    return r;
}

gp::A11yReport failedScanReport() {
    gp::A11yReport r;
    r.path = QStringLiteral("broken.pdf");
    r.loadOk = false;
    r.loadError = QStringLiteral("Cannot load: parse error");
    return r;
}

QString wholeText(PdfiumBackend& reader) {
    QString all;
    const int n = reader.pageCount();
    for (int p = 0; p < n; ++p)
        all += reader.extractText(p) + QLatin1Char('\n');
    // Wrapped lines (wrapText at ~92 chars) split phrases across extracted
    // text lines; whitespace-normalize so phrase pins see the sentence.
    return all.simplified();
}

} // namespace

class TestA11yReportWriter : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // The SafeSave commit-fault seam is process-global — reset it around
    // EVERY test so a failure mid-pin can never poison a later one.
    void init() {
        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::None);
    }
    void cleanup() {
        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::None);
    }

    // ── the CSV escaping contract (single-contract discipline) ──────────
    void csvCellEscapingContract() {
        // Formula-injection hardening: the PGR-16 lead characters escape,
        // delegated to ConversionManager::csvFormulaSafeCell — the SAME
        // contract the conversion and comments exporters use, so the three
        // exporters cannot drift.
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("=cmd|' /C calc'")),
                 QStringLiteral("'=cmd|' /C calc'"));
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("@SUM(1)")),
                 QStringLiteral("'@SUM(1)"));
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("+3+cmd")),
                 QStringLiteral("'+3+cmd"));
        // M3 exemption: a PLAIN number is not a formula.
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("-2")),
                 QStringLiteral("-2"));
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("+3.14")),
                 QStringLiteral("+3.14"));
        // The PGR-16 fixture still escapes (sign + operator is not a number).
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("-2+3+cmd")),
                 QStringLiteral("'-2+3+cmd"));
        // RFC-4180: separators, quotes and newlines force quoting with inner
        // quotes doubled.
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("a,b")),
                 QStringLiteral("\"a,b\""));
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("say \"hi\"")),
                 QStringLiteral("\"say \"\"hi\"\"\""));
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("line1\nline2")),
                 QStringLiteral("\"line1\nline2\""));
        // A benign cell passes through untouched.
        QCOMPARE(gp::A11yReportWriter::csvCell(QStringLiteral("page 3 · image /Im0")),
                 QStringLiteral("page 3 · image /Im0"));
    }

    // ── CSV of a loaded report: verdict, degradations, findings ─────────
    void csvCarriesHonestVerdictFindingsAndDegradations() {
        const QString out =
            m_tmpDir.filePath(QStringLiteral("a11y-report.csv"));
        QString err;
        QVERIFY2(gp::A11yReportWriter::writeCsv(out, loadedReport(), &err),
                 qPrintable(err));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString csv = QString::fromUtf8(f.readAll());
        f.close();

        // Machine-readable format tag + report facts.
        QVERIFY2(csv.contains(QStringLiteral("glyphpdf-a11y-report/1")),
                 "format tag");
        QVERIFY2(csv.contains(QStringLiteral("contract.pdf")), "document");
        QVERIFY2(csv.contains(QStringLiteral("Scan status,ok")), "scan status");
        QVERIFY2(csv.contains(QStringLiteral("Tagged,false")), "tagged");
        QVERIFY2(csv.contains(QStringLiteral("Finding count,4")),
                 "finding count");
        QVERIFY2(csv.contains(QStringLiteral("High severity,2")), "high count");
        QVERIFY2(csv.contains(QStringLiteral("Medium severity,1")),
                 "medium count");
        QVERIFY2(csv.contains(QStringLiteral("Low severity,1")), "low count");
        // Capability degradation: the bounded sample with its disclosed total.
        QVERIFY2(csv.contains(QStringLiteral("Fields reported,60 of 100")),
                 "field truncation disclosed");
        QVERIFY2(csv.contains(QStringLiteral("Truncated,true")),
                 "truncated flag");
        // Honest verdict + standing disclosure — never a conformance claim.
        QVERIFY2(csv.contains(
                     QStringLiteral("Verdict,4 gap(s) found — report truncated")),
                 qPrintable(csv));
        QVERIFY2(csv.contains(QStringLiteral("not a PDF/UA verdict")),
                 "disclosure");

        // Findings table: header + one row per finding, page 1-based,
        // machine targets carried. Cells without separators stay unquoted
        // (the RFC-4180 rule quotes only when the cell carries '"', ',' or
        // a newline).
        QVERIFY2(csv.contains(
                     QStringLiteral("Severity,Check,Page,Target,Where,Detail")),
                 "findings header");
        QVERIFY2(csv.contains(
                     QStringLiteral("HIGH,struct-tree,,,document,"
                                   "No structure tree")),
                 "struct-tree row");
        QVERIFY2(csv.contains(QStringLiteral("HIGH,image-alt,3,Im0")),
                 "image-alt row (1-based page + target id)");
        QVERIFY2(csv.contains(QStringLiteral("MEDIUM,doc-language,")),
                 "doc-language row");
        QVERIFY2(csv.contains(QStringLiteral("LOW,display-doc-title,")),
                 "display-doc-title row");
        // A findings-free report NEVER appears as "accessible"/"conformant".
        QVERIFY2(!csv.contains(QStringLiteral("accessible")),
                 "no accessibility claim");
        QVERIFY2(!csv.contains(QStringLiteral("conformant")),
                 "no conformance claim");
    }

    // ── CSV of a clean scan: zero findings is NOT a PDF/UA verdict ──────
    void csvCleanScanNeverClaimsConformance() {
        const QString out =
            m_tmpDir.filePath(QStringLiteral("a11y-clean.csv"));
        QString err;
        QVERIFY2(gp::A11yReportWriter::writeCsv(out, cleanReport(), &err),
                 qPrintable(err));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString csv = QString::fromUtf8(f.readAll());
        f.close();

        QVERIFY2(csv.contains(
                     QStringLiteral("Verdict,No gaps found by these checks")),
                 "honest zero-findings wording");
        QVERIFY2(csv.contains(QStringLiteral("Tagged,true")), "tagged flag");
        QVERIFY2(csv.contains(QStringLiteral("Truncated,false")),
                 "not truncated");
        // The standing disclosure travels with the verdict.
        QVERIFY2(csv.contains(QStringLiteral("not a PDF/UA verdict")),
                 "disclosure present");
    }

    // ── CSV of a FAILED scan: exported as failed, never greenwashed ─────
    void csvFailedScanIsExportedAsFailed() {
        const QString out =
            m_tmpDir.filePath(QStringLiteral("a11y-failed.csv"));
        QString err;
        QVERIFY2(gp::A11yReportWriter::writeCsv(out, failedScanReport(), &err),
                 qPrintable(err));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString csv = QString::fromUtf8(f.readAll());
        f.close();

        QVERIFY2(csv.contains(QStringLiteral("Scan status,failed")),
                 "failed status");
        QVERIFY2(csv.contains(QStringLiteral("Cannot load: parse error")),
                 "load error carried");
        QVERIFY2(csv.contains(
                     QStringLiteral("Verdict,Scan failed")),
                 "verdict says failed");
        // No finding rows: the checker produced NONE — none may be invented.
        QVERIFY2(!csv.contains(QStringLiteral("HIGH,")), "no invented rows");
        QVERIFY2(!csv.contains(QStringLiteral("MEDIUM,")),
                 "no invented rows");
    }

    // ── CSV formula-injection end to end: attacker-shaped finding text ──
    void csvFormulaInjectionHardenedEndToEnd() {
        gp::A11yReport r = loadedReport();
        // An attacker-influenceable string (document-derived) leading with a
        // formula trigger must reach the file apostrophe-prefixed (the cell
        // carries no separator, so it stays unquoted per RFC-4180).
        r.findings.first().whyNot =
            QStringLiteral("=cmd|' /C calc'!A0 — injected?");

        const QString out =
            m_tmpDir.filePath(QStringLiteral("a11y-injection.csv"));
        QString err;
        QVERIFY2(gp::A11yReportWriter::writeCsv(out, r, &err), qPrintable(err));

        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString csv = QString::fromUtf8(f.readAll());
        f.close();
        QVERIFY2(csv.contains(QStringLiteral("'=cmd|' /C calc'!A0 — injected?")),
                 "formula payload escaped at the emission boundary");
    }

    // ── PDF: readable artifact carrying verdict + findings + disclosure ─
    void pdfSummaryCarriesVerdictFindingsAndDisclosure() {
        const QString out =
            m_tmpDir.filePath(QStringLiteral("a11y-summary.pdf"));
        QString err;
        QVERIFY2(gp::A11yReportWriter::writePdf(
                     out, QStringLiteral("contract.pdf"), loadedReport(), &err),
                 qPrintable(err));
        QVERIFY(QFileInfo::exists(out));

        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        const QString text = wholeText(reader);

        QVERIFY2(text.contains(QStringLiteral("Accessibility Check Summary")),
                 "title");
        QVERIFY2(text.contains(QStringLiteral("contract.pdf")), "document");
        QVERIFY2(text.contains(QStringLiteral("Generated: ")),
                 "generation timestamp");
        QVERIFY2(text.contains(QStringLiteral("4 gap(s) found")),
                 "honest verdict");
        QVERIFY2(text.contains(QStringLiteral("not a PDF/UA verdict")),
                 "standing disclosure");
        QVERIFY2(text.contains(QStringLiteral("60 of 100")),
                 "truncation disclosed");
        QVERIFY2(text.contains(QStringLiteral("No structure tree")),
                 "finding detail");
        QVERIFY2(text.contains(QStringLiteral("[image-alt] page 3 · image /Im0")),
                 "finding target line");
        QVERIFY2(text.contains(QStringLiteral("HIGH")), "severity labels");
        QVERIFY2(text.contains(QStringLiteral("MEDIUM")), "severity labels");
        QVERIFY2(text.contains(QStringLiteral("LOW")), "severity labels");
    }

    // ── PDF: clean scan and failed scan stay honest on paper too ────────
    void pdfCleanAndFailedScansAreHonest() {
        {
            const QString out =
                m_tmpDir.filePath(QStringLiteral("a11y-clean.pdf"));
            QString err;
            QVERIFY2(gp::A11yReportWriter::writePdf(
                         out, QStringLiteral("clean.pdf"), cleanReport(), &err),
                     qPrintable(err));
            PdfiumBackend reader;
            QVERIFY(reader.loadDocument(out));
            const QString text = wholeText(reader);
            QVERIFY2(text.contains(
                         QStringLiteral("No gaps found by these checks")),
                     "honest zero-findings wording");
            QVERIFY2(text.contains(QStringLiteral("not a PDF/UA verdict")),
                     "disclosure");
            QVERIFY2(!text.contains(QStringLiteral("conformant")),
                     "no conformance claim");
        }
        {
            const QString out =
                m_tmpDir.filePath(QStringLiteral("a11y-failed.pdf"));
            QString err;
            QVERIFY2(gp::A11yReportWriter::writePdf(
                         out, QStringLiteral("broken.pdf"), failedScanReport(),
                         &err),
                     qPrintable(err));
            PdfiumBackend reader;
            QVERIFY(reader.loadDocument(out));
            const QString text = wholeText(reader);
            QVERIFY2(text.contains(QStringLiteral("Scan failed")),
                     "verdict says failed");
            QVERIFY2(text.contains(QStringLiteral("Cannot load: parse error")),
                     "load error carried");
            QVERIFY2(!text.contains(QStringLiteral("No gaps found")),
                     "a failed scan never reads as clean");
        }
    }

    // ── verdictLine seam: the exact honest sentences, no PDF round-trip ─
    void verdictLineWordingIsPinned() {
        QVERIFY2(gp::A11yReportWriter::verdictLine(loadedReport())
                     .startsWith(QStringLiteral("4 gap(s) found")),
                 "count verdict");
        QVERIFY(gp::A11yReportWriter::verdictLine(loadedReport())
                    .contains(QStringLiteral("report truncated")));
        QCOMPARE(gp::A11yReportWriter::verdictLine(cleanReport()),
                 QStringLiteral("No gaps found by these checks. This is not a "
                                "PDF/UA verdict — content tagging is not "
                                "checked."));
        QVERIFY2(gp::A11yReportWriter::verdictLine(failedScanReport())
                     .startsWith(QStringLiteral("Scan failed:")),
                 "failed verdict");
        QVERIFY(gp::A11yReportWriter::verdictLine(failedScanReport())
                    .contains(QStringLiteral("no findings are available")));
    }

    // ── overwrite guard: a refused commit leaves the destination intact ─
    void failedCommitLeavesDestinationByteIdentical() {
        const QString out =
            m_tmpDir.filePath(QStringLiteral("a11y-guard.pdf"));
        QVERIFY(gp::A11yReportWriter::writePdf(
            out, QStringLiteral("contract.pdf"), loadedReport(), nullptr));
        QFile before(out);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const QByteArray payload = before.readAll();
        before.close();
        QVERIFY(!payload.isEmpty());

        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        QString err;
        QVERIFY2(!gp::A11yReportWriter::writePdf(out, QStringLiteral("x.pdf"),
                                                 loadedReport(), &err),
                 "the faulted commit must fail the export");
        QFile after(out);
        QVERIFY(after.open(QIODevice::ReadOnly));
        const QByteArray payloadAfter = after.readAll();
        after.close();
        QCOMPARE(payloadAfter, payload);
    }
};

QTEST_MAIN(TestA11yReportWriter)
#include "TestA11yReportWriter.moc"
