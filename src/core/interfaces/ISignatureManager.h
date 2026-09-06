// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>
#include <QDateTime>
#include <QRectF>

struct SignatureInfo {
    QString fieldName;
    QString signerName;
    QString reason;
    QString location;
    QDateTime date;
    bool integrityIntact = false;
    bool isValid = false;
    QString trustStatus;
    QString trustStoreUsed;
    // PAdES-level fields
    bool hasDss = false;           // B-LT: DSS dictionary present
    bool hasDocTimestamp = false;  // B-LTA: document timestamp present
    // NF-6: set to true when an OCSP response was found in the DSS but the
    // original OCSP request object is unavailable at validation time, so nonce
    // verification (OCSP_check_nonce) was skipped. Closed as "documented +
    // deferred to M5 VRI work" when the request is persisted alongside the
    // response.
    bool ocspNoteNF6 = false;
    // ER-1: "NoCertMatch" when DSS /OCSPs entries exist but none of their
    // single responses match the signer certificate's serial + issuer hash.
    // Caller maps this to "UntrustedChain" in trustStatus.
    QString ocspStatus;
};

/// PAdES signature conformance levels per ETSI EN 319 132-1.
enum class PAdESLevel {
    B_B,   ///< Basic - CAdES signature only (no timestamp)
    B_T,   ///< With timestamp token (/ByteRange RFC 3161 token)
    B_LT,  ///< Long-Term - B-T plus DSS dictionary (OCSP + certs + CRLs)
    B_LTA  ///< Long-Term with archive timestamp (/DocTimeStamp)
};

/// Outcome of the most recent signDocument/certifyDocument call. Lets the UI tell
/// a TOTAL failure (nothing usable written) apart from a PARTIAL one where the
/// cryptographic signature bytes ARE on disk but the requested long-term-validation
/// data (B-LT DSS / B-LTA archive timestamp) could not be embedded (audit E-02).
enum class SignOutcome {
    NotRun,             ///< No signing attempted in this manager yet.
    Failed,             ///< Total failure — no valid signature was written.
    Success,            ///< Fully successful at the requested PAdES level.
    PartialLtvMissing   ///< Core signature written, but B-LT/B-LTA data is missing.
};

/// §9.7 P1: WHY the most recent outcome was PartialLtvMissing — exactly which
/// long-term-validation enhancement could not be embedded. A partial result
/// with NO flagged piece is impossible; at least one flag is set whenever the
/// outcome is PartialLtvMissing.
struct SignatureOutcomeDetail {
    bool dssMissing = false;          ///< B-LT: the DSS dictionary could not be built/embedded
    bool docTimestampMissing = false; ///< B-LTA: the /DocTimeStamp could not be added
};

class ISignatureManager {
public:
    virtual ~ISignatureManager() = default;

    /// Set the RFC 3161 timestamp authority URL.
    virtual void setTsaUrl(const QString &url) = 0;

    /// Set the target PAdES conformance level (default: B_T).
    virtual void setSignatureLevel(PAdESLevel level) = 0;

    virtual SignOutcome signDocument(const QString &inputPath, const QString &outputPath,
                               const QString &certPath, const QString &password,
                               const QString &reason = QString(), const QString &location = QString()) = 0;

    virtual SignOutcome certifyDocument(const QString &inputPath, const QString &outputPath,
                                 const QString &certPath, const QString &password,
                                 int certificationLevel = 1,
                                 const QString &reason = QString(), const QString &location = QString()) = 0;

    virtual bool addDocTimeStamp(const QString &inputPath, const QString &outputPath) = 0;

    virtual QList<SignatureInfo> validateSignatures(const QString &filePath) = 0;

    /// §9.7 P1: detail of the most recent signDocument/certifyDocument outcome.
    /// Deliberately NON-pure with this default body: implementations that do
    /// not track degradation detail (including test mocks) compile unchanged
    /// and simply report "no detail" instead of being forced to stub it.
    virtual SignatureOutcomeDetail lastSignOutcomeDetail()
    {
        return {};   // no degradation detail known by this implementation
    }

    // §9.7 P0 (badge anchoring): on-page anchor for every signature field —
    // fieldName matches SignatureInfo::fieldName; rect is the widget /Rect in
    // viewer top-left convention (PDF y flipped). Empty list when the document
    // has no signature fields or cannot be read.
    struct SignatureFieldAnchor {
        QString fieldName;
        int pageIndex = -1;
        QRectF rect;
    };
    virtual QList<SignatureFieldAnchor> signatureFieldAnchors(const QString &filePath) = 0;

protected:
    ISignatureManager() = default;
    ISignatureManager(const ISignatureManager&) = delete;
    ISignatureManager& operator=(const ISignatureManager&) = delete;
};
