#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace gp {

/**
 * Lightweight auto-update system for GlyphPDF (Session 18).
 *
 * Flow:
 *   1. checkForUpdates() — async GET of manifest JSON
 *   2. If newer version → updateAvailable() signal with release info
 *   3. User clicks Download → downloadUpdate()
 *   4. Download finishes → SHA-256 verified → downloadReady()
 *   5. User clicks Install → applyUpdate() → launches MSI, app exits
 *
 * Safety:
 *   - Never auto-downloads without user action
 *   - Never auto-installs
 *   - SHA-256 verified before offering install
 *   - Failed download/install leaves current version intact (D5)
 */
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    struct UpdateInfo {
        QString version;
        QString releaseDate;
        QUrl    downloadUrl;
        QString sha256;
        QString releaseNotes;
        QString minVersion;     // minimum version that can upgrade
    };

    // Configuration
    void setManifestUrl(const QUrl& url);
    QUrl manifestUrl() const { return m_manifestUrl; }

    // Current state
    bool isChecking() const { return m_checking; }
    bool isDownloading() const { return m_downloading; }
    int  downloadProgress() const { return m_progressPct; }
    UpdateInfo latestUpdate() const { return m_latest; }
    QString downloadedMsiPath() const { return m_downloadedPath; }

    static QString currentVersion();

public slots:
    /** Fetch manifest and compare versions. Non-blocking. */
    void checkForUpdates();

    /** Download MSI to temp directory. Emits downloadProgress/downloadReady. */
    void downloadUpdate();

    /** Launch downloaded MSI in silent upgrade mode and request app exit. */
    void applyUpdate();

signals:
    void checkStarted();
    void updateAvailable(const UpdateInfo& info);
    void noUpdateAvailable();
    void checkFailed(const QString& reason);

    void downloadStarted();
    void downloadProgressChanged(int percent);
    void downloadReady(const QString& msiPath);
    void downloadFailed(const QString& reason);

    void updateLaunched();   // MSI started — app should exit now

private slots:
    void onManifestReply();
    void onDownloadProgress(qint64 received, qint64 total);
    void onDownloadFinished();

private:
    bool isNewerVersion(const QString& remote, const QString& local) const;
    bool verifySha256(const QString& filePath, const QString& expected) const;

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply*         m_manifestReply = nullptr;
    QNetworkReply*         m_downloadReply = nullptr;

    QUrl       m_manifestUrl;
    UpdateInfo m_latest;
    QString    m_downloadedPath;

    bool m_checking    = false;
    bool m_downloading = false;
    int  m_progressPct = 0;

    static constexpr const char* DefaultManifestUrl =
        "https://glyphpdf.com/updates/latest.json";
};

} // namespace gp
