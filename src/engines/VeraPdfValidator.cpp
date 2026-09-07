// SPDX-License-Identifier: Apache-2.0
#include "VeraPdfValidator.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace gp {

// Runtime discovery of the veraPDF CLI (AGPL-3.0 — invoked as a subprocess only,
// never linked in-process). Detection happens at runtime, not build time, so the
// shipped binary finds a veraPDF that is either bundled alongside GlyphPDF or
// installed by the user — independent of what the build machine had.
QString VeraPdfValidator::locateCli() {
    const QString appDir = QCoreApplication::applicationDirPath();
    // 1. Bundled next to the app (deploy.ps1 stages it under verapdf/).
    const QStringList bundled = {
        appDir + "/verapdf/verapdf",
        appDir + "/verapdf/verapdf.bat",
        appDir + "/verapdf",
        appDir + "/verapdf.bat",
    };
    for (const QString& p : bundled) {
        if (QFileInfo::exists(p))
            return QDir::toNativeSeparators(p);
    }
    // 2. Explicit override via environment variable.
    const QString envPath = qEnvironmentVariable("GLYPHPDF_VERAPDF");
    if (!envPath.isEmpty() && QFileInfo::exists(envPath))
        return QDir::toNativeSeparators(envPath);
    // 3. On the PATH.
    for (const QString& exe : {QStringLiteral("verapdf"), QStringLiteral("verapdf.bat")}) {
        const QString onPath = QStandardPaths::findExecutable(exe);
        if (!onPath.isEmpty())
            return QDir::toNativeSeparators(onPath);
    }
    return QString();
}

bool VeraPdfValidator::isAvailable() {
    return !locateCli().isEmpty();
}

QString VeraPdfValidator::conformanceFlag(PdfAConformance level) {
    switch (level) {
        case PdfAConformance::PDF_A_1B: return "1b";
        case PdfAConformance::PDF_A_2B: return "2b";
        case PdfAConformance::PDF_A_3B: return "3b";
        case PdfAConformance::PDF_A_2U: return "2u";
        case PdfAConformance::PDF_A_3U: return "3u";
    }
    return "2b";
}

PdfAValidationReport VeraPdfValidator::validate(const QString& pdfPath, PdfAConformance level) {
    PdfAValidationReport report;
    report.validatorAvailable = false;

    // H-1: the previous blocklist only rejected & | < >, but the validator runs a
    // .bat via `cmd.exe /c call`, where %VAR% environment expansion and the ^ escape
    // are ALSO interpreted by the shell, and ( ) " can break batch argument parsing.
    // pdfPath is the user-opened document path, so a crafted name like
    //   %SystemRoot%\..\evil & calc.bat
    // could inject. Switch to a STRICT ALLOWLIST: reject any path containing a
    // shell/batch metacharacter. Legitimate PDF paths never need these.
    {
        static const QString kForbidden = QStringLiteral("%^\"&|<>()");
        for (const QChar c : pdfPath) {
            if (kForbidden.contains(c)) {
                report.errorMessage = "Invalid characters in PDF path";
                return report;
            }
        }
    }

    const QString cliPath = locateCli();
    if (cliPath.isEmpty()) {
        report.errorMessage = QStringLiteral(
            "veraPDF validator not found. Install veraPDF (https://verapdf.org) "
            "or place it next to GlyphPDF to enable PDF/A validation.");
        return report;
    }

    report.validatorAvailable = true;

    QProcess proc;
    QStringList args;
    if (cliPath.endsWith(".bat", Qt::CaseInsensitive) || cliPath.endsWith(".cmd", Qt::CaseInsensitive)) {
        args << "/c" << "call" << cliPath;
        args << "--format" << "json" << "--flavour" << conformanceFlag(level) << pdfPath;
        proc.start("cmd.exe", args);
    } else {
        args << "--format" << "json" << "--flavour" << conformanceFlag(level) << pdfPath;
        proc.start(cliPath, args);
    }

    if (!proc.waitForStarted(5000)) {
        report.errorMessage = "veraPDF failed to start";
        return report;
    }

    if (!proc.waitForFinished(30000)) { // 30s timeout
        proc.kill();
        report.errorMessage = QStringLiteral("veraPDF timed out after 30 seconds.");
        return report;
    }

    QByteArray output = proc.readAllStandardOutput();
    PdfAValidationReport r = parseJson(output);
    r.validatorAvailable = true;
    if (!r.errorMessage.isEmpty() && proc.exitCode() != 0) {
        // Keep the specific cause (taskException / parse detail) and append
        // the exit code rather than replacing it.
        r.errorMessage += QStringLiteral(" (veraPDF exit code %1)").arg(proc.exitCode());
    }
    return r;
}

PdfAValidationReport VeraPdfValidator::parseJson(const QByteArray& jsonOutput) {
    PdfAValidationReport report;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonOutput, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        report.errorMessage = QStringLiteral("Failed to parse veraPDF output: ") + err.errorString();
        return report;
    }

    // veraPDF 1.26–1.30 JSON schema (mirrors TestBatchOpsCoverage::runVeraPdfRaw):
    //   { "report": { "jobs": [ {
    //       "taskException"?: { "message": "..." }   ← document not even parseable
    //       "validationResult": [ {                  ← ARRAY, one entry per profile
    //           "profileName": "PDF/A-2B validation profile",
    //           "status": "passed" | "failed",
    //           "details": {
    //               "passedRules": N, "failedRules": M,    ← INTEGER counts
    //               "passedChecks": A, "failedChecks": B,  ← INTEGER counts
    //               "ruleSummaries": [ { "clause": "6.6.2.1", "testNumber": 1,
    //                   "status": "failed", "failedChecks": 1,
    //                   "checks": [ { "status": "failed",
    //                       "errorMessage": "..." } ] } ] } } ] } ] }
    // The pre-1.26 reading (validationResult as an object with "result":
    // "passed", failedChecks as an array) matches NO released veraPDF — with
    // it the validator reported isValid=false, violations=[] for every file.
    QJsonObject root = doc.object();
    QJsonArray jobs = root["report"].toObject()["jobs"].toArray();
    if (jobs.isEmpty()) {
        report.errorMessage = QStringLiteral("veraPDF returned no jobs in output.");
        return report;
    }

    QJsonObject job = jobs.first().toObject();

    // A taskException means veraPDF could not process the document at all —
    // never a silent verdict: surface a diagnostic, keep isValid=false.
    if (job.contains(QStringLiteral("taskException"))) {
        const QString message =
            job["taskException"].toObject()["message"].toString();
        report.errorMessage = QStringLiteral("veraPDF could not process the document "
                                             "(taskException)") +
                              (message.isEmpty() ? QStringLiteral(".")
                                                 : QStringLiteral(": ") + message);
        return report;
    }

    int failedRulesTotal = 0;
    int failedChecksTotal = 0;
    const QJsonArray results = job["validationResult"].toArray();
    for (const QJsonValue& resultVal : results) {
        const QJsonObject result = resultVal.toObject();
        const QString profileName = result["profileName"].toString();
        if (!profileName.isEmpty())
            report.conformanceLevel = profileName;

        const QJsonObject details = result["details"].toObject();
        failedRulesTotal += details["failedRules"].toInt(0);
        failedChecksTotal += details["failedChecks"].toInt(0);

        const QJsonArray ruleSummaries = details["ruleSummaries"].toArray();
        for (const QJsonValue& summaryVal : ruleSummaries) {
            const QJsonObject rule = summaryVal.toObject();
            if (rule["status"].toString().compare(QStringLiteral("failed"),
                                                  Qt::CaseInsensitive) != 0)
                continue;

            RuleViolation v;
            v.clause = rule["clause"].toString();
            v.ruleId = QStringLiteral("%1-%2")
                           .arg(v.clause, rule["testNumber"].toVariant().toString());
            v.severity = QStringLiteral("error"); // failed rules are always errors
            v.pageNumber = -1; // JSON output locations carry no page number

            QStringList messages;
            for (const QJsonValue& checkVal : rule["checks"].toArray()) {
                const QJsonObject check = checkVal.toObject();
                const QString message = check["errorMessage"].toString();
                if (!message.isEmpty())
                    messages << message;
                const int page =
                    check["location"].toObject()["pageNumber"].toInt(-1);
                if (page >= 0 && v.pageNumber < 0)
                    v.pageNumber = page;
            }
            // Occurrence count fallback when the CLI emitted no per-check
            // error messages for this rule.
            v.description = messages.isEmpty()
                ? QStringLiteral("Rule %1 failed (%2 occurrence(s))")
                      .arg(v.ruleId).arg(rule["failedChecks"].toInt(0))
                : messages.join(QStringLiteral("; "));
            report.violations.append(v);
        }
    }

    // Definitive verdict: valid only when a document was processed without a
    // taskException AND zero failed checks / rules.
    report.isValid = (failedChecksTotal == 0 && failedRulesTotal == 0);
    if (report.isValid)
        report.violations.clear(); // never report violations on a PASS verdict
    return report;
}

} // namespace gp
