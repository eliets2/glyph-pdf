#include "VeraPdfValidator.h"
#include <QFile>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// VERAPDF_CLI_PATH is injected by CMake when -DVERAPDF_CLI_PATH=... is set.
// Without it, isAvailable() returns false and validate() returns a "unavailable" report.
#ifndef VERAPDF_CLI_PATH
#define VERAPDF_CLI_PATH ""
#endif

namespace gp {

static const QString kCliPath = QString::fromUtf8(VERAPDF_CLI_PATH);

bool VeraPdfValidator::isAvailable() {
    return !kCliPath.isEmpty() && QFile::exists(kCliPath);
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

    if (!isAvailable()) {
        report.errorMessage = QStringLiteral(
            "veraPDF validator not configured. "
            "Build with -DVERAPDF_CLI_PATH=/path/to/verapdf to enable.");
        return report;
    }

    report.validatorAvailable = true;

    QProcess proc;
    QStringList args = {
        "--format", "json",
        "--flavour", conformanceFlag(level),
        pdfPath
    };
    proc.start(kCliPath, args);
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
