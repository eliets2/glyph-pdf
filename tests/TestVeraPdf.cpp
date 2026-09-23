#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/VeraPdfValidator.h"

// TestVeraPdf — veraPDF CLI subprocess integration tests + schema-parser pins.
//
// veraPDF is located at runtime (VeraPdfValidator::locateCli): a bundled copy,
// the GLYPHPDF_VERAPDF env var (aliased from the test-lane spelling
// GLYPHPDF_VERAPDF_CLI), or the PATH. When none is present, isAvailable()
// returns false and the CLI-dependent test functions QSKIP individually —
// the parseJson fixture tests below are CLI-independent and ALWAYS run.
//
// SCHEMA (pinned here, implemented in VeraPdfValidator::parseJson): the real
// veraPDF 1.26–1.30 JSON —
//   { "report": { "jobs": [ {
//       "taskException"?: { "message": "..." }     ← document not parseable at all
//       "validationResult": [ {                    ← ARRAY of per-profile entries
//           "profileName": "PDF/A-2B validation profile",
//           "status": "passed" | "failed",
//           "details": {
//               "passedRules": N, "failedRules": M,      ← INTEGER counts
//               "passedChecks": A, "failedChecks": B,    ← INTEGER counts
//               "ruleSummaries": [ { "clause": "6.6.2.1", "testNumber": 1,
//                   "status": "failed", "failedChecks": 1,
//                   "checks": [ { "status": "failed",
//                       "errorMessage": "..." } ] } ] } } ] } ] }
// The pre-fix parser read validationResult as an OBJECT ("result": "passed")
// and failedChecks as an ARRAY — a shape no released veraPDF emits — so the
// validator reported isValid=false, violations=[] for EVERY document. This
// file previously documented that defect (commit fee597b); the fix lives in
// src/engines/VeraPdfValidator.cpp and is pinned here offline.
//
// WHAT IS / IS NOT VALIDATED HERE: offline: the parser contract (violations
// carry clause-testNumber rule ids, isValid only when no exception and zero
// failed checks/rules). With a CLI: the subprocess pipeline (discovery, .bat
// handling, timeouts, parse-error tolerance) and the REAL conformance verdict
// of a non-conformant document, cross-checked against the independent raw-CLI
// parse below (same shape as TestBatchOpsCoverage's helper — each test
// executable is standalone, no shared test library to host it in).

using namespace gp;

namespace {

struct VeraPdfRawVerdict {
    bool ran = false;
    bool jsonOk = false;
    bool definite = false;   // no taskException
    bool valid = false;      // failedRules == 0
    QStringList failedClauses;
    QStringList messages;
    QString error;
};

VeraPdfRawVerdict runVeraPdfRaw(const QString& pdfPath, const QString& flavourFlag) {
    VeraPdfRawVerdict v;
    const QString cli = VeraPdfValidator::locateCli();
    if (cli.isEmpty()) {
        v.error = QStringLiteral("veraPDF CLI not found");
        return v;
    }
    v.ran = true;

    QProcess proc;
    QStringList args;
    if (cli.endsWith(QStringLiteral(".bat"), Qt::CaseInsensitive) ||
        cli.endsWith(QStringLiteral(".cmd"), Qt::CaseInsensitive)) {
        args << QStringLiteral("/c") << QStringLiteral("call") << cli
             << QStringLiteral("--format") << QStringLiteral("json")
             << QStringLiteral("--flavour") << flavourFlag << pdfPath;
        proc.start(QStringLiteral("cmd.exe"), args);
    } else {
        args << QStringLiteral("--format") << QStringLiteral("json")
             << QStringLiteral("--flavour") << flavourFlag << pdfPath;
        proc.start(cli, args);
    }
    if (!proc.waitForStarted(15000)) {
        v.error = QStringLiteral("veraPDF CLI failed to start");
        return v;
    }
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        v.error = QStringLiteral("veraPDF CLI timed out after 60 seconds");
        return v;
    }

    QJsonParseError parseErr;
    const QJsonDocument doc =
        QJsonDocument::fromJson(proc.readAllStandardOutput(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        v.error = QStringLiteral("veraPDF output is not JSON: ") + parseErr.errorString();
        return v;
    }
    const QJsonArray jobs =
        doc.object()[QStringLiteral("report")].toObject()[QStringLiteral("jobs")].toArray();
    if (jobs.isEmpty()) {
        v.error = QStringLiteral("veraPDF JSON has no jobs");
        return v;
    }
    v.jsonOk = true;

    const QJsonObject job = jobs.first().toObject();
    if (job.contains(QStringLiteral("taskException"))) {
        v.error = QStringLiteral("veraPDF could not parse the document (taskException)");
        return v;
    }
    v.definite = true;

    int failedRulesTotal = 0;
    const QJsonArray results = job[QStringLiteral("validationResult")].toArray();
    for (const QJsonValue& rv : results) {
        const QJsonObject details = rv.toObject()[QStringLiteral("details")].toObject();
        failedRulesTotal += details[QStringLiteral("failedRules")].toInt(0);
        const QJsonArray summaries = details[QStringLiteral("ruleSummaries")].toArray();
        for (const QJsonValue& sv : summaries) {
            const QJsonObject rule = sv.toObject();
            if (rule[QStringLiteral("status")].toString()
                    .compare(QLatin1String("failed"), Qt::CaseInsensitive) != 0)
                continue;
            v.failedClauses << QStringLiteral("%1-%2")
                    .arg(rule[QStringLiteral("clause")].toString(),
                         rule[QStringLiteral("testNumber")].toVariant().toString());
            for (const QJsonValue& cv : rule[QStringLiteral("checks")].toArray())
                v.messages << cv.toObject()[QStringLiteral("errorMessage")].toString();
        }
    }
    v.valid = (failedRulesTotal == 0);
    return v;
}

// ── Offline fixtures: real veraPDF 1.26–1.30 JSON shapes ────────────────────

// Non-conformant document: one passed rule and two failed rules with checks.
constexpr char kFailedDocJson[] = R"json({
    "report": { "jobs": [ {
        "id": "job-1",
        "fileName": "plain.pdf",
        "validationResult": [ {
            "profileName": "PDF/A-2B validation profile",
            "status": "failed",
            "details": {
                "passedRules": 96, "failedRules": 2,
                "passedChecks": 1201, "failedChecks": 3,
                "ruleSummaries": [
                    { "clause": "6.6.2.1", "testNumber": 1,
                      "status": "failed", "failedChecks": 1,
                      "checks": [ { "status": "failed",
                          "errorMessage": "The document catalog does not contain the pdfaid identification",
                          "location": { "level": "Document",
                              "relativePath": [ { "level": "Document", "name": "root" } ] } } ] },
                    { "clause": "6.1.11", "testNumber": 1,
                      "status": "failed", "failedChecks": 2,
                      "checks": [ { "status": "failed",
                          "errorMessage": "The document does not contain an XMP metadata stream" } ] },
                    { "clause": "6.1.13", "testNumber": 7,
                      "status": "passed", "failedChecks": 0,
                      "checks": [] }
                ] } } ] } ] }
})json";

// Conformant document: zero failed rules / checks, status "passed".
constexpr char kPassedDocJson[] = R"json({
    "report": { "jobs": [ {
        "id": "job-2",
        "fileName": "good.pdf",
        "validationResult": [ {
            "profileName": "PDF/A-2B validation profile",
            "status": "passed",
            "details": {
                "passedRules": 98, "failedRules": 0,
                "passedChecks": 1204, "failedChecks": 0,
                "ruleSummaries": [
                    { "clause": "6.1.13", "testNumber": 7,
                      "status": "passed", "failedChecks": 0,
                      "checks": [] }
                ] } } ] } ] }
})json";

// Document veraPDF could not even open: job carries a taskException.
constexpr char kTaskExceptionJson[] = R"json({
    "report": { "jobs": [ {
        "id": "job-3",
        "fileName": "broken.pdf",
        "taskException": {
            "message": "FileNotFoundException: broken.pdf (The system cannot find the file specified)",
            "nextAction": "Check the file path",
            "batchResult": { "totalJobs": 1, "failedJobs": 1 } },
        "validationResult": []
    } ] }
})json";

// Failed rule where the CLI emitted only the occurrence count (failedChecks
// INTEGER) without a checks array — the violation must still surface.
constexpr char kOccurrencesOnlyJson[] = R"json({
    "report": { "jobs": [ {
        "validationResult": [ {
            "profileName": "PDF/A-1B validation profile",
            "status": "failed",
            "details": {
                "passedRules": 51, "failedRules": 1,
                "passedChecks": 640, "failedChecks": 3,
                "ruleSummaries": [
                    { "clause": "6.2.3.3", "testNumber": 2,
                      "status": "failed", "failedChecks": 3,
                      "checks": [] }
                ] } } ] } ] }
})json";

} // namespace

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
        // E-1: honor both spellings of the optional veraPDF CLI override —
        // GLYPHPDF_VERAPDF_CLI (test-lane spelling) aliases the app's
        // GLYPHPDF_VERAPDF consumed by VeraPdfValidator::locateCli().
        // NOTE: no QSKIP here — the parseJson fixture tests below pin the
        // real CLI schema offline and must run even without veraPDF.
        const QByteArray cliAlias = qgetenv("GLYPHPDF_VERAPDF_CLI");
        if (!cliAlias.isEmpty() && qEnvironmentVariableIsEmpty("GLYPHPDF_VERAPDF"))
            qputenv("GLYPHPDF_VERAPDF", cliAlias);
        QVERIFY(m_tmpDir.isValid());
    }

    // ── Offline parseJson fixtures (no CLI required) ────────────────────────

    // Failed document: isValid=false, violations carry the failed rules'
    // clause-testNumber ids, passed rules are not reported.
    void testParseJsonFailedDocViolationsCarryRuleIds() {
        auto report = VeraPdfValidator::parseJson(QByteArray(kFailedDocJson));

        QVERIFY2(report.errorMessage.isEmpty(),
                 qPrintable(QStringLiteral("clean JSON must not set errorMessage: ")
                            + report.errorMessage));
        QVERIFY2(!report.isValid,
                 "document with failedChecks=3 must be invalid");
        QCOMPARE(report.conformanceLevel, QStringLiteral("PDF/A-2B validation profile"));
        QCOMPARE(report.violations.size(), 2);

        const RuleViolation& v0 = report.violations[0];
        QCOMPARE(v0.ruleId, QStringLiteral("6.6.2.1-1"));
        QCOMPARE(v0.clause, QStringLiteral("6.6.2.1"));
        QVERIFY2(v0.description.contains(QLatin1String("pdfaid")),
                 qPrintable(QStringLiteral("violation description must carry the check "
                                           "errorMessage: ") + v0.description));
        QCOMPARE(v0.severity, QStringLiteral("error"));
        QCOMPARE(v0.pageNumber, -1); // JSON output carries no page number

        const RuleViolation& v1 = report.violations[1];
        QCOMPARE(v1.ruleId, QStringLiteral("6.1.11-1"));
        QCOMPARE(v1.clause, QStringLiteral("6.1.11"));
        QVERIFY(v1.description.contains(QLatin1String("XMP metadata")));

        // A passed rule must never surface as a violation.
        for (const RuleViolation& v : report.violations)
            QVERIFY2(v.ruleId != QStringLiteral("6.1.13-7"),
                     "passed rule 6.1.13-7 must not be reported as a violation");
    }

    // Conformant document: isValid=true exactly when failedChecks==0 and no
    // taskException, with an empty violations list.
    void testParseJsonPassedDocIsValid() {
        auto report = VeraPdfValidator::parseJson(QByteArray(kPassedDocJson));

        QVERIFY(report.errorMessage.isEmpty());
        QVERIFY2(report.isValid,
                 "document with failedChecks=0 and no taskException must be valid");
        QVERIFY(report.violations.isEmpty());
        QCOMPARE(report.conformanceLevel, QStringLiteral("PDF/A-2B validation profile"));
    }

    // taskException means veraPDF could not process the document at all:
    // isValid=false AND a diagnostic errorMessage (never a silent verdict).
    void testParseJsonTaskExceptionIsInvalidWithMessage() {
        auto report = VeraPdfValidator::parseJson(QByteArray(kTaskExceptionJson));

        QVERIFY2(!report.isValid, "taskException must yield isValid=false");
        QVERIFY2(!report.errorMessage.isEmpty(),
                 "taskException must produce a diagnostic errorMessage");
        QVERIFY(report.errorMessage.contains(QLatin1String("taskException")));
        QVERIFY(report.violations.isEmpty());
    }

    // Failed rule with only the occurrence count (no checks array): the
    // violation must still surface, carrying the rule id and occurrences.
    void testParseJsonOccurrencesOnlyRuleSurfaces() {
        auto report = VeraPdfValidator::parseJson(QByteArray(kOccurrencesOnlyJson));

        QVERIFY(report.errorMessage.isEmpty());
        QVERIFY2(!report.isValid, "failedChecks=3 must yield isValid=false");
        QCOMPARE(report.violations.size(), 1);
        QCOMPARE(report.violations[0].ruleId, QStringLiteral("6.2.3.3-2"));
        QCOMPARE(report.violations[0].clause, QStringLiteral("6.2.3.3"));
        QVERIFY2(report.violations[0].description.contains(QLatin1String("3")),
                 qPrintable(QStringLiteral("description should mention the 3 occurrences: ")
                            + report.violations[0].description));
    }

    // Garbage output must set errorMessage (and never a silent isValid).
    void testParseJsonMalformedOutputSetsError() {
        auto report = VeraPdfValidator::parseJson(QByteArray("<html>500</html>"));

        QVERIFY2(!report.isValid, "unparseable output must yield isValid=false");
        QVERIFY2(!report.errorMessage.isEmpty(),
                 "unparseable output must set errorMessage");
        QVERIFY(report.violations.isEmpty());

        // jobs-less but well-formed JSON is equally undiagnosable.
        auto empty = VeraPdfValidator::parseJson(
            QByteArrayLiteral("{\"report\": {\"jobs\": []}}"));
        QVERIFY(!empty.isValid);
        QVERIFY2(!empty.errorMessage.isEmpty(),
                 "empty jobs array must set errorMessage");
    }

    // ── CLI integration (individually skipped when veraPDF is absent) ───────

    // isAvailable() must be true when we reach here (skipped otherwise)
    void testValidatorAvailabilityReflected() {
        if (!VeraPdfValidator::isAvailable())
            QSKIP("veraPDF not found at runtime (bundle/env/PATH)");
        QVERIFY(VeraPdfValidator::isAvailable());
    }

    // A plain PDF (no PDF/A metadata) must fail PDF/A-2b validation with at
    // least one failed rule. The verdict is read from the REAL CLI JSON via
    // the independent raw helper, then the in-app pipeline is cross-checked
    // against it in testValidatorPipelineReturnsReport.
    void testMalformedPdfReportsViolations() {
        if (!VeraPdfValidator::isAvailable())
            QSKIP("veraPDF not found at runtime (bundle/env/PATH)");
        QString path = createPlainPdf();
        const VeraPdfRawVerdict raw = runVeraPdfRaw(path, QStringLiteral("2b"));

        QVERIFY2(raw.ran && raw.jsonOk,
                 qPrintable(QStringLiteral("CLI must run and return JSON: %1").arg(raw.error)));
        QVERIFY2(raw.definite,
                 qPrintable(QStringLiteral("plain PDF must at least parse (no "
                                           "taskException): %1").arg(raw.error)));
        QVERIFY2(!raw.valid,
                 "Plain PDF must not conform to PDF/A-2b");
        QVERIFY2(!raw.failedClauses.isEmpty(),
                 "At least one failed RULE must be reported for a plain PDF");

        // The known plain-PDF failure classes: missing catalog/XMP metadata
        // identification (clause 6.6.x) and writer object-spacing syntax.
        qInfo().noquote() << "[E-1] plain-PDF failed rules:"
                          << raw.failedClauses.join(QStringLiteral(", "));
        for (const QString& msg : raw.messages)
            QVERIFY2(!msg.trimmed().isEmpty(),
                     "every reported check must carry a non-empty message");
    }

    // The in-app pipeline must run the CLI and — with the real-schema
    // parseJson — produce a definitive verdict for a plain PDF: isValid=false
    // with violations whose rule ids exactly match the independent raw parse.
    void testValidatorPipelineReturnsReport() {
        if (!VeraPdfValidator::isAvailable())
            QSKIP("veraPDF not found at runtime (bundle/env/PATH)");
        QString path = createPlainPdf();
        auto report = VeraPdfValidator::validate(path, PdfAConformance::PDF_A_2B);

        QVERIFY2(report.validatorAvailable,
            "validatorAvailable should be true when CLI is present");
        QVERIFY2(report.errorMessage.isEmpty(),
                 qPrintable(QStringLiteral("a well-formed plain PDF must produce a "
                                           "definite verdict, got error: ")
                            + report.errorMessage));
        QVERIFY2(!report.isValid,
                 "plain PDF must not conform to PDF/A-2b through the in-app parser");
        QVERIFY2(!report.violations.isEmpty(),
                 "in-app parser must surface failed rules as violations");

        // Cross-check against the independent raw-CLI verdict (same file).
        const VeraPdfRawVerdict raw = runVeraPdfRaw(path, QStringLiteral("2b"));
        QVERIFY2(raw.definite,
                 qPrintable(QStringLiteral("raw verdict must be definite: %1").arg(raw.error)));
        QStringList appIds;
        for (const RuleViolation& v : report.violations)
            appIds << v.ruleId;
        QStringList rawIds = raw.failedClauses;
        appIds.sort();
        rawIds.sort();
        QCOMPARE(appIds, rawIds);
    }

    // Passing a nonexistent file path should not crash — the validator is
    // available (CLI found) but veraPDF emits a taskException for the file it
    // cannot open, which must surface as a diagnostic (never a silent verdict
    // with an empty violations list).
    void testNonExistentFileDoesNotCrash() {
        if (!VeraPdfValidator::isAvailable())
            QSKIP("veraPDF not found at runtime (bundle/env/PATH)");
        auto report = VeraPdfValidator::validate(
            tmpPath("does_not_exist_12345.pdf"),
            PdfAConformance::PDF_A_2B);

        QVERIFY(report.validatorAvailable);
        // isValid is false when file not found; the critical requirements:
        // no exception, no crash, and a diagnosable outcome (error message or
        // explicit violations — not the old silent isValid=false/[] pair).
        QVERIFY(!report.isValid);
        QVERIFY2(!report.errorMessage.isEmpty() || !report.violations.isEmpty(),
                 "unopenable file must yield a diagnostic, not a silent verdict");
    }
};

QTEST_GUILESS_MAIN(TestVeraPdf)
#include "TestVeraPdf.moc"
