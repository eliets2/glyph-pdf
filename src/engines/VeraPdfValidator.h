#pragma once
#include <QString>
#include <QList>

namespace gp {

enum class PdfAConformance {
    PDF_A_1B,
    PDF_A_2B,
    PDF_A_3B,
    PDF_A_2U,
    PDF_A_3U
};

struct RuleViolation {
    QString ruleId;
    QString clause;
    QString description;
    int pageNumber = -1;
    QString severity; // "error" | "warning"
};

struct PdfAValidationReport {
    bool isValid = false;
    QString conformanceLevel;
    QList<RuleViolation> violations;
    QString errorMessage; // non-empty if validator unavailable or crashed
    bool validatorAvailable = false;
};

class VeraPdfValidator {
public:
    static PdfAValidationReport validate(const QString& pdfPath, PdfAConformance level);
    static bool isAvailable();

private:
    static QString conformanceFlag(PdfAConformance level);
    static PdfAValidationReport parseJson(const QByteArray& jsonOutput);
};

} // namespace gp
