#pragma once
#include "core/interfaces/ISignatureManager.h"

class MockSignatureManager : public ISignatureManager {
public:
    void setTsaUrl(const QString &url) override { m_tsaUrl = url; }

    void setSignatureLevel(PAdESLevel level) override { m_level = level; }

    SignOutcome signDocument(const QString &inputPath, const QString &outputPath,
                      const QString &certPath, const QString &password,
                      const QString &reason = QString(), const QString &location = QString()) override {
        m_lastInputPath  = inputPath;
        m_lastOutputPath = outputPath;
        m_lastCertPath   = certPath;
        m_lastPassword   = password;
        m_lastReason     = reason;
        m_lastLocation   = location;
        ++m_signCalls;
        QFile::copy(inputPath, outputPath);
        return m_signResult ? SignOutcome::Success : SignOutcome::Failed;
    }

    SignOutcome certifyDocument(const QString &inputPath, const QString &outputPath,
                         const QString &certPath, const QString &password,
                         int certificationLevel = 1,
                         const QString &reason = QString(), const QString &location = QString()) override {
        m_lastInputPath  = inputPath;
        m_lastOutputPath = outputPath;
        m_lastCertPath   = certPath;
        m_lastPassword   = password;
        m_lastReason     = reason;
        m_lastLocation   = location;
        ++m_signCalls; // count as a sign call
        QFile::copy(inputPath, outputPath);
        return m_signResult ? SignOutcome::Success : SignOutcome::Failed;
    }

    bool addDocTimeStamp(const QString &inputPath, const QString &outputPath) override {
        m_lastInputPath = inputPath;
        m_lastOutputPath = outputPath;
        return m_signResult;
    }

    QList<SignatureFieldAnchor> signatureFieldAnchors(const QString &filePath) override {
        Q_UNUSED(filePath);
        return m_anchors;
    }
    QList<SignatureFieldAnchor> m_anchors;
    QList<SignatureInfo> validateSignatures(const QString &filePath) override {
        m_lastInputPath = filePath;
        ++m_validateCalls;
        return m_signatures;
    }

    // ── configurable return values ──────────────────────────────────────────
    bool m_signResult = true;
    QList<SignatureInfo> m_signatures;
    PAdESLevel m_level = PAdESLevel::B_T;

    // ── call tracking ───────────────────────────────────────────────────────
    int m_signCalls     = 0;
    int m_validateCalls = 0;
    QString m_lastInputPath;
    QString m_lastOutputPath;
    QString m_lastCertPath;
    QString m_lastPassword;
    QString m_lastReason;
    QString m_lastLocation;
    QString m_tsaUrl;
};
