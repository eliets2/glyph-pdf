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
        appDir + "/verapdf/verapdf.bat",
        appDir + "/verapdf/verapdf",
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

    const QString cliPath = locateCli();
    if (cliPath.isEmpty()) {
        report.errorMessage = QStringLiteral(
            "veraPDF validator not found. Install veraPDF (https://verapdf.org) "
            "or place it next to GlyphPDF to enable PDF/A validation.");
        return report;
    }

    report.validatorAvailable = true;

    QProcess proc;
    QStringList args = {
        "--format", "json",
        "--flavour", conformanceFlag(level),
        pdfPath
    };
    proc.start(cliPath, args);
    if (!proc.waitForFinished(30000)) { // 30s timeout
        proc.kill();
        report.errorMessage = QStringLiteral("veraPDF timed out after 30 seconds.");
        return report;
    }

    QByteArray output = proc.readAllStandardOutput();
    return parseJson(output);
}

PdfAValidationReport VeraPdfValidator::parseJson(const QByteArray& jsonOutput) {
    PdfAValidationReport report;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonOutput, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        report.errorMessage = QStringLiteral("Failed to parse veraPDF output: ") + err.errorString();
        return report;
    }

    // veraPDF JSON: { "report": { "jobs": [ { "validationResult": { ... } } ] } }
    QJsonObject root = doc.object();
    QJsonObject reportObj = root["report"].toObject();
    QJsonArray jobs = reportObj["jobs"].toArray();
    if (jobs.isEmpty()) {
        report.errorMessage = QStringLiteral("veraPDF returned no jobs in output.");
        return report;
    }

    QJsonObject job = jobs.first().toObject();
    QJsonObject valResult = job["validationResult"].toObject();

    report.isValid = (valResult["result"].toString() == "passed");
    report.conformanceLevel = valResult["profileName"].toString();

    QJsonArray failedChecks = valResult["failedChecks"].toArray();
    for (const QJsonValue& checkVal : failedChecks) {
        QJsonObject check = checkVal.toObject();
        RuleViolation v;
        QJsonObject ruleId = check["ruleId"].toObject();
        v.clause = ruleId["clause"].toString();
        v.ruleId = v.clause + "-" + ruleId["testNumber"].toString();
        v.description = check["message"].toString();
        QJsonObject loc = check["location"].toObject();
        v.pageNumber = loc["pageNumber"].toInt(-1);
        v.severity = QStringLiteral("error"); // veraPDF failedChecks are always errors
        report.violations.append(v);
    }

    return report;
}

} // namespace gp
