#pragma once
#include "core/interfaces/ISignatureManager.h"

class MockSignatureManager : public ISignatureManager {
public:
    void setTsaUrl(const QString &url) override { m_tsaUrl = url; }

    bool signDocument(const QString &inputPath, const QString &outputPath,
                      const QString &certPath, const QString &password,
                      const QString &reason, const QString &location) override {
        m_lastInputPath = inputPath;
        m_lastOutputPath = outputPath;
        m_lastCertPath = certPath;
        m_lastPassword = password;
        m_lastReason = reason;
        m_lastLocation = location;
        ++m_signCalls;
        return m_signResult;
    }

    QList<SignatureInfo> validateSignatures(const QString &filePath) override {
        m_lastInputPath = filePath;
        ++m_validateCalls;
        return m_signatures;
    }

    // Test helpers -- configurable return values
    bool m_signResult = true;
    QList<SignatureInfo> m_signatures;

    // Test helpers -- call tracking
    int m_signCalls = 0;
    int m_validateCalls = 0;
    QString m_lastInputPath;
    QString m_lastOutputPath;
    QString m_lastCertPath;
    QString m_lastPassword;
    QString m_lastReason;
    QString m_lastLocation;
    QString m_tsaUrl;
};
