#pragma once

#include <QString>
#include <QList>
#include <QDateTime>
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

    bool signDocument(const QString &inputPath,
                      const QString &outputPath,
                      const QString &certPath,
                      const QString &password,
                      const QString &reason = QString(),
                      const QString &location = QString()) override;

    bool certifyDocument(const QString &inputPath,
                         const QString &outputPath,
                         const QString &certPath,
                         const QString &password,
                         int certificationLevel = 1,
                         const QString &reason = QString(),
                         const QString &location = QString()) override;

    bool addDocTimeStamp(const QString &inputPath,
                         const QString &outputPath) override;

    /**
     * @brief Validate all digital signatures in a PDF.
     * @param filePath Path to the PDF file.
     * @return List of signature information.
     */
    QList<SignatureInfo> validateSignatures(const QString &filePath) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};
