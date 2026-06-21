// SPDX-License-Identifier: Apache-2.0
#include "core/UpdateChecker.h"
#include "core/TempFileManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSettings>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#endif

namespace gp {

// ── ctor / dtor ──────────────────────────────────────────────────────────

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    QSettings s;
    QString url = s.value("update/manifestUrl",
                          QString::fromLatin1(DefaultManifestUrl)).toString();
    // Security: enforce HTTPS for the manifest URL — fall back to default if a
    // non-HTTPS scheme has been persisted (e.g. malicious QSettings tampering).
    if (QUrl(url).scheme().toLower() != "https") {
        qWarning() << "UpdateChecker: persisted manifest URL is not HTTPS, falling back to default:"
                   << url;
        url = QString::fromLatin1(DefaultManifestUrl);
    }
    m_manifestUrl = QUrl(url);
}

UpdateChecker::~UpdateChecker() {
    // Clean up any in-flight replies
    if (m_manifestReply) { m_manifestReply->abort(); m_manifestReply->deleteLater(); }
    if (m_downloadReply) { m_downloadReply->abort(); m_downloadReply->deleteLater(); }
}

// ── Configuration ────────────────────────────────────────────────────────

void UpdateChecker::setManifestUrl(const QUrl& url) {
    // Security: only accept HTTPS manifest URLs. Reject and surface the
    // rejection via checkFailed so the caller can react.
    if (url.scheme().toLower() != "https") {
        emit checkFailed(QStringLiteral("Update manifest URL must use HTTPS scheme (rejected: %1)")
                             .arg(url.toString()));
        return;
    }
    m_manifestUrl = url;
}

QString UpdateChecker::currentVersion() {
    return QCoreApplication::applicationVersion();
}

QUrl UpdateChecker::manifestUrlForChannel(const QString& channel) {
    if (channel.compare(QLatin1String("beta"), Qt::CaseInsensitive) == 0)
        return QUrl(QStringLiteral("https://eliets2.github.io/glyph-pdf/updates/beta.json"));
    return QUrl(QString::fromLatin1(DefaultManifestUrl));
}

// ── Version comparison ───────────────────────────────────────────────────

bool UpdateChecker::isNewerVersion(const QString& remote, const QString& local) const {
    // Semantic versioning comparison: "1.2.3" > "0.2.0"
    const auto rParts = remote.split('.');
    const auto lParts = local.split('.');
    int count = qMax(rParts.size(), lParts.size());
    for (int i = 0; i < count; ++i) {
        int r = (i < rParts.size()) ? rParts[i].toInt() : 0;
        int l = (i < lParts.size()) ? lParts[i].toInt() : 0;
        if (r > l) return true;
        if (r < l) return false;
    }
    return false; // equal
}

// ── SHA-256 verification ─────────────────────────────────────────────────

bool UpdateChecker::verifySha256(const QString& filePath, const QString& expected) const {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return false;

    QString actual = hash.result().toHex().toLower();
    QString expect = expected.toLower().trimmed();

    if (actual != expect) {
        qWarning() << "UpdateChecker: SHA-256 mismatch!"
                   << "expected:" << expect
                   << "actual:" << actual;
        return false;
    }
    return true;
}

// ── D1: Check for updates ────────────────────────────────────────────────

void UpdateChecker::checkForUpdates() {
    if (m_checking) return;
    m_checking = true;
    emit checkStarted();

    QNetworkRequest req(m_manifestUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("GlyphPDF/%1").arg(currentVersion()));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    m_manifestReply = m_nam->get(req);
    connect(m_manifestReply, &QNetworkReply::finished,
            this, &UpdateChecker::onManifestReply);
}

void UpdateChecker::onManifestReply() {
    m_checking = false;
    auto* reply = m_manifestReply;
    m_manifestReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(tr("Could not reach the update server: %1").arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (doc.isNull()) {
        emit checkFailed(tr("Invalid update manifest: %1").arg(parseErr.errorString()));
        return;
    }

    QJsonObject obj = doc.object();
    m_latest.version     = obj.value("version").toString();
    m_latest.releaseDate = obj.value("releaseDate").toString();
    m_latest.downloadUrl = QUrl(obj.value("downloadUrl").toString());
    m_latest.sha256      = obj.value("sha256").toString();
    m_latest.releaseNotes= obj.value("releaseNotes").toString();
    m_latest.minVersion  = obj.value("minVersion").toString();

    if (m_latest.version.isEmpty()) {
        emit checkFailed(tr("Update manifest missing version field."));
        return;
    }

    // B-03: a poisoned manifest could specify an http (or otherwise non-TLS)
    // downloadUrl and/or omit the sha256, after which the file would be fetched
    // over an unauthenticated channel, the (conditional) integrity check skipped,
    // and msiexec run on attacker-controlled bytes — a remote-code-execution path.
    // Refuse such a manifest here, BEFORE advertising the update, so neither the
    // download nor msiexec can ever be reached for it. Require:
    //   (a) a valid downloadUrl whose scheme is exactly https, and
    //   (b) a non-empty sha256 so onDownloadFinished's integrity check is mandatory.
    if (!m_latest.downloadUrl.isValid() ||
        m_latest.downloadUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
        qWarning() << "UpdateChecker: rejecting manifest — downloadUrl is not https:"
                   << m_latest.downloadUrl.toString();
        emit checkFailed(tr("Update rejected: the download URL is not a secure (https) link."));
        return;
    }
    if (m_latest.sha256.trimmed().isEmpty()) {
        qWarning() << "UpdateChecker: rejecting manifest — empty sha256 (integrity"
                   << "check would be skipped).";
        emit checkFailed(tr("Update rejected: the manifest is missing a SHA-256 checksum."));
        return;
    }

    // Check if remote version is newer than current
    if (!isNewerVersion(m_latest.version, currentVersion())) {
        emit noUpdateAvailable();
        return;
    }

    // Check minimum version constraint — current must be >= minVersion
    if (!m_latest.minVersion.isEmpty() &&
        isNewerVersion(m_latest.minVersion, currentVersion())) {
        emit checkFailed(
            tr("This version (%1) is too old to auto-update to %2. "
               "Please download the latest installer manually.")
                .arg(currentVersion(), m_latest.version));
        return;
    }

    emit updateAvailable(m_latest);
}

// ── D2: Download with progress ───────────────────────────────────────────

void UpdateChecker::downloadUpdate() {
    if (m_downloading) return;
    if (!m_latest.downloadUrl.isValid()) {
        emit downloadFailed(tr("No valid download URL available."));
        return;
    }

    m_downloading = true;
    m_progressPct = 0;
    emit downloadStarted();

    // Create tracked temp file for the MSI
    m_downloadedPath = TempFileManager::instance().createTempFile(".msi");
    if (m_downloadedPath.isEmpty()) {
        m_downloading = false;
        emit downloadFailed(tr("Could not create temporary file for download."));
        return;
    }

    QNetworkRequest req(m_latest.downloadUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("GlyphPDF/%1").arg(currentVersion()));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    m_downloadReply = m_nam->get(req);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &UpdateChecker::onDownloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished,
            this, &UpdateChecker::onDownloadFinished);
}

void UpdateChecker::onDownloadProgress(qint64 received, qint64 total) {
    if (total > 0) {
        m_progressPct = static_cast<int>(received * 100 / total);
    } else {
        m_progressPct = -1; // indeterminate
    }
    emit downloadProgressChanged(m_progressPct);
}

void UpdateChecker::onDownloadFinished() {
    m_downloading = false;
    auto* reply = m_downloadReply;
    m_downloadReply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // D5: Rollback — remove partial download, continue running
        TempFileManager::instance().untrack(m_downloadedPath);
        QFile::remove(m_downloadedPath);
        m_downloadedPath.clear();
        emit downloadFailed(tr("Download failed: %1").arg(reply->errorString()));
        return;
    }

    // Write to disk
    QFile f(m_downloadedPath);
    if (!f.open(QIODevice::WriteOnly)) {
        m_downloadedPath.clear();
        emit downloadFailed(tr("Could not write downloaded update to disk."));
        return;
    }
    f.write(reply->readAll());
    f.close();

    // B-03: SHA-256 verification is MANDATORY (fail-closed). An empty checksum must
    // never silently skip the check — onManifestReply already refuses an empty
    // sha256, and this guard ensures no other entry point can reach msiexec with an
    // unverified package.
    if (m_latest.sha256.trimmed().isEmpty() ||
        !verifySha256(m_downloadedPath, m_latest.sha256)) {
        TempFileManager::instance().untrack(m_downloadedPath);
        QFile::remove(m_downloadedPath);
        m_downloadedPath.clear();
        emit downloadFailed(
            tr("Update package failed integrity check (SHA-256 mismatch or missing "
               "checksum). The download was discarded. Please try again later."));
        return;
    }

    m_progressPct = 100;
    emit downloadProgressChanged(100);
    emit downloadReady(m_downloadedPath);
}

// ── D3: Apply update ─────────────────────────────────────────────────────

void UpdateChecker::applyUpdate() {
    if (m_downloadedPath.isEmpty() || !QFile::exists(m_downloadedPath)) {
        qWarning() << "UpdateChecker::applyUpdate: no downloaded MSI available";
        return;
    }

#ifdef Q_OS_WIN
    // N-2 FIX: Verify Authenticode signature before launching MSI.
    // SHA-256 hash alone is insufficient if the manifest server is compromised.
    std::wstring wDownloaded = m_downloadedPath.toStdWString();
    HANDLE hMsi = CreateFileW(wDownloaded.c_str(), GENERIC_READ,
                              FILE_SHARE_READ,          // deny write/delete sharing while we hold it
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hMsi == INVALID_HANDLE_VALUE) {
    // Re-verify SHA-256 after handle is opened to prevent TOCTOU
    if (!verifySha256(m_downloadedPath, m_latest.sha256)) {
        CloseHandle(hMsi);
        qWarning() << "N-2: TOCTOU SHA-256 mismatch before apply for" << m_downloadedPath;
        emit checkFailed(tr("Update file integrity check failed at install time."));
        return;
    }

        qWarning() << "N-2: could not open MSI for verification:" << m_downloadedPath;
        emit checkFailed(tr("Update file could not be opened for signature verification."));
        return;
    }
    if (!verifyAuthenticode(hMsi, m_downloadedPath)) {
        CloseHandle(hMsi);
        qWarning() << "N-2: Authenticode check failed - aborting and deleting" << m_downloadedPath;
        QFile::remove(m_downloadedPath);
        emit checkFailed(tr("Update file signature verification failed. "
                            "The downloaded file may be corrupted or tampered with. "
                            "Please try again or download manually from the official website."));
        return;
    }
    qInfo() << "N-2: Authenticode verification passed for" << m_downloadedPath;
#else
    // SECFIX-3 Part B: no Authenticode equivalent on non-Windows - refuse to launch
    // an installer we cannot verify, rather than falling through to msiexec.
    qWarning() << "N-2: Non-Windows platform - refusing to launch MSI installer";
    return;
#endif

    // Launch MSI in silent upgrade mode via msiexec
    // /qb = basic UI (progress bar only)
    // /i  = install (MajorUpgrade in WiX handles the upgrade)
    QStringList args = {
        "/i", m_downloadedPath,
        "/qb",
        "REBOOT=ReallySuppress"   // don't reboot automatically
    };

    bool started = QProcess::startDetached("msiexec.exe", args);
#ifdef Q_OS_WIN
    CloseHandle(hMsi);   // SECFIX-3: release only after launch; no swap window remained open
#endif

    if (started) {
        emit updateLaunched();
        // The MainWindow should connect to this signal and call qApp->quit()
    } else {
        qWarning() << "UpdateChecker: failed to launch msiexec";
        // D5: Rollback — app continues running, user can try again
    }
}

#ifdef Q_OS_WIN
bool UpdateChecker::verifyAuthenticode(void* hFile, const QString& pathForLogging)
{
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct       = sizeof(WINTRUST_FILE_INFO);
    // SECFIX-3: verify the OPEN HANDLE, not the path. hFile takes precedence and
    // closes the TOCTOU window between verify and msiexec launch. pcwszFilePath is
    // left null so WinVerifyTrust cannot re-open (and an attacker cannot swap) the path.
    fileInfo.pcwszFilePath  = nullptr;
    fileInfo.hFile          = hFile;
    fileInfo.pgKnownSubject = nullptr;

    WINTRUST_DATA trustData{};
    trustData.cbStruct            = sizeof(WINTRUST_DATA);
    trustData.pPolicyCallbackData = nullptr;
    trustData.pSIPClientData      = nullptr;
    trustData.dwUIChoice          = WTD_UI_NONE;          // no UI prompts
    trustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN; // check full chain
    trustData.dwUnionChoice       = WTD_CHOICE_FILE;
    trustData.pFile               = &fileInfo;
    trustData.dwStateAction       = WTD_STATEACTION_VERIFY;
    trustData.hWVTStateData       = nullptr;
    trustData.pwszURLReference    = nullptr;
    trustData.dwProvFlags         = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    trustData.dwUIContext         = 0;

    GUID actionGuid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG result = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                                  &actionGuid,
                                  &trustData);

    if (result == ERROR_SUCCESS) {
        bool publisherMatch = false;
        CRYPT_PROVIDER_DATA* provData = WTHelperProvDataFromStateData(trustData.hWVTStateData);
        if (provData) {
            CRYPT_PROVIDER_SGNR* provSigner = WTHelperGetProvSignerFromChain(provData, 0, FALSE, 0);
            if (provSigner && provSigner->csCertChain > 0) {
                PCCERT_CONTEXT cert = provSigner->pasCertChain[0].pCert;
                DWORD nameLen = CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
                if (nameLen > 0) {
                    std::vector<wchar_t> nameBuf(nameLen);
                    CertGetNameStringW(cert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nameBuf.data(), nameLen);
                    QString publisher = QString::fromWCharArray(nameBuf.data());
                    if (publisher.contains("GlyphPDF", Qt::CaseInsensitive)) {
                        publisherMatch = true;
                    } else {
                        qWarning() << "N-2: Authenticode publisher mismatch. Expected GlyphPDF, got:" << publisher;
                    }
                }
            }
        }
        if (!publisherMatch) {
            result = TRUST_E_SUBJECT_NOT_TRUSTED;
        }
    }

    // Close the state data to release resources
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &actionGuid, &trustData);

    if (result != ERROR_SUCCESS) {
        qWarning() << "N-2: Authenticode verification failed for" << pathForLogging
                   << "— HRESULT:" << Qt::hex << result;
        return false;
    }
    return true;
}
#endif

} // namespace gp
