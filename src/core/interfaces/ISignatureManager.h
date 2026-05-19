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
};

class ISignatureManager {
public:
    virtual ~ISignatureManager() = default;
    virtual void setTsaUrl(const QString &url) = 0;
    virtual bool signDocument(const QString &inputPath, const QString &outputPath,
                               const QString &certPath, const QString &password,
                               const QString &reason = QString(), const QString &location = QString()) = 0;
    virtual QList<SignatureInfo> validateSignatures(const QString &filePath) = 0;
protected:
    ISignatureManager() = default;
    ISignatureManager(const ISignatureManager&) = delete;
    ISignatureManager& operator=(const ISignatureManager&) = delete;
};
