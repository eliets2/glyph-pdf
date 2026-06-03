// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>
#include <QDateTime>

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
    bool hasDss = false;        // B-LT: DSS dictionary present
    bool hasDocTimestamp = false; // B-LTA: document timestamp present
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

class ISignatureManager {
public:
    virtual ~ISignatureManager() = default;

    /// Set the RFC 3161 timestamp authority URL.
    virtual void setTsaUrl(const QString &url) = 0;

    /// Set the target PAdES conformance level (default: B_T).
    virtual void setSignatureLevel(PAdESLevel level) = 0;

    virtual bool signDocument(const QString &inputPath, const QString &outputPath,
                               const QString &certPath, const QString &password,
                               const QString &reason = QString(), const QString &location = QString()) = 0;

    virtual bool certifyDocument(const QString &inputPath, const QString &outputPath,
                                 const QString &certPath, const QString &password,
                                 int certificationLevel = 1,
                                 const QString &reason = QString(), const QString &location = QString()) = 0;

    virtual bool addDocTimeStamp(const QString &inputPath, const QString &outputPath) = 0;

    /// Outcome of the most recent signDocument/certifyDocument call. The UI uses
    /// this to differentiate a partial (LTV-missing) result from a total failure
    /// (E-02). Default is NotRun for managers that do not track it.
    virtual SignOutcome lastSignOutcome() const { return SignOutcome::NotRun; }

    virtual QList<SignatureInfo> validateSignatures(const QString &filePath) = 0;

protected:
    ISignatureManager() = default;
    ISignatureManager(const ISignatureManager&) = delete;
    ISignatureManager& operator=(const ISignatureManager&) = delete;
};
