// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
#include <QImage>
#include <functional>
#include <memory>
#include "core/interfaces/ISignatureManager.h"

struct x509_store_st;
typedef struct x509_store_st X509_STORE;

class SignatureManager final : public ISignatureManager
{
public:
    SignatureManager();
    ~SignatureManager() override;

    void setTrustStoreForTest(X509_STORE *store);

    // ------------------------------------------------------------------
    // §9.7 P0 — visible signature appearance (ETSI EN 319 142-6 §5.2)
    // ------------------------------------------------------------------
    // The planned content of one /AP /N form XObject: the text lines to draw
    // (top-to-bottom), the chosen font size, and layout flags.
    struct SignatureAppearancePlan {
        QStringList lines;      // draw order; empty => image-only / nothing
        double fontSize = 0.0;  // pt; > 0 exactly when lines is non-empty
        bool nameOnly = false;  // identity line only (tiny rect / no-fit fallback)
        bool imageLeft = false; // signature image occupies the left panel
    };

    // Deterministic text-width measurer: (text, fontSize) -> width in pt.
    // The engine passes the real standard-14 Helvetica metrics; tests and the
    // dialog preview pass their own (QFontMetricsF / synthetic).
    using AppearanceMeasureFn = std::function<double(const QString &, double)>;

    // Pure layout planner — the testable seam for the appearance content.
    // Ladder (binding design, research-remaining-p0.md Lane A):
    //   1. try the full line set ("Digitally signed by {CN}", "Date: ...",
    //      "Reason: ..." / "Location: ..." ONLY when set) at 9..6pt;
    //   2. drop Reason, re-try; 3. drop Location, re-try;
    //   4. rects < ~120x36pt (or still no fit) render name-only, shrinking
    //      to an absolute 4pt floor so the identity line is never lost.
    // Non-Latin caveat: the engine renders with standard-14 Helvetica
    // (WinAnsi) — see drawSignatureAppearance in the .cpp.
    static SignatureAppearancePlan planSignatureAppearance(
        double rectWidthPt, double rectHeightPt,
        const QString &signerName,
        const QDateTime &claimedLocalTime,
        const QString &reason, const QString &location,
        bool hasSignatureImage,
        const AppearanceMeasureFn &measureTextWidth);

    // Dialog -> engine handoff for the OPTIONAL signature image (picked in
    // SignatureDialog via SignatureContent::loadUploaded). Consume-once: the
    // next signDocument/certifyDocument call embeds it (left panel of the
    // appearance) and drains the slot, so no stale image can leak into an
    // unrelated later signature. A null image clears the slot.
    static void setPendingAppearanceImage(const QImage &image);
    static QImage takePendingAppearanceImage();

    /**
     * @brief Sign a PDF document using an X.509 certificate (P12/PFX).
     * @param inputPath Source PDF file.
     * @param outputPath Destination PDF file.
     * @param certPath Path to .p12 or .pfx certificate.
     * @param password Password for the certificate.
     * @return true if successful.
     */
    void setTsaUrl(const QString &url) override;
    void setSignatureLevel(PAdESLevel level) override;

    SignOutcome signDocument(const QString &inputPath,
                      const QString &outputPath,
                      const QString &certPath,
                      const QString &password,
                      const QString &reason = QString(),
                      const QString &location = QString()) override;

    SignOutcome certifyDocument(const QString &inputPath,
                         const QString &outputPath,
                         const QString &certPath,
                         const QString &password,
                         int certificationLevel = 1,
                         const QString &reason = QString(),
                         const QString &location = QString()) override;

    bool addDocTimeStamp(const QString &inputPath, const QString &outputPath) override;

    /**
     * @brief Validate all digital signatures in a PDF.
     * @param filePath Path to the PDF file.
     * @return List of signature information.
     */
    QList<SignatureInfo> validateSignatures(const QString &filePath) override;

private:
    static bool isLegitimateIncrementalAppend(const QByteArray& trailingBytes,
                                              const QByteArray& baseDocument,
                                              QString& reason);

    // Shared signing core used by both signDocument (certificationLevel == 0) and
    // certifyDocument (certificationLevel 1..3 -> /DocMDP). See SignatureManager.cpp.
    SignOutcome signDocumentImpl(const QString &inputPath,
                          const QString &outputPath,
                          const QString &certPath,
                          const QString &password,
                          int certificationLevel,
                          const QString &reason,
                          const QString &location);

    class Private;
    std::unique_ptr<Private> d;
};
