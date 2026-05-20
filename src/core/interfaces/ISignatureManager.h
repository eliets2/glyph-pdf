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

    virtual QList<SignatureInfo> validateSignatures(const QString &filePath) = 0;

protected:
    ISignatureManager() = default;
    ISignatureManager(const ISignatureManager&) = delete;
    ISignatureManager& operator=(const ISignatureManager&) = delete;
};
