// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>

// Test binary's QtTest suite — declared only so it can be friended below
// (test-only access to the private schema parser; no production code path).
class TestVeraPdf;

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

    // Runtime path to the veraPDF CLI (bundled copy, env override, or PATH).
    // Empty when no validator is present. Resolved at runtime, never at build time.
    static QString locateCli();

private:
    static QString conformanceFlag(PdfAConformance level);
    static PdfAValidationReport parseJson(const QByteArray& jsonOutput);

    // Test-only access to the private schema parser: TestVeraPdf pins the real
    // veraPDF 1.26-1.30 JSON shape with offline fixtures (no CLI required).
    friend class ::TestVeraPdf;
};

} // namespace gp
