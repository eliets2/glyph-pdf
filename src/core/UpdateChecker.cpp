#include "core/UpdateChecker.h"
#include "core/TempFileManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
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

    // SHA-256 verification
    if (!m_latest.sha256.isEmpty()) {
        if (!verifySha256(m_downloadedPath, m_latest.sha256)) {
            TempFileManager::instance().untrack(m_downloadedPath);
            QFile::remove(m_downloadedPath);
            m_downloadedPath.clear();
            emit downloadFailed(
                tr("Update package failed integrity check (SHA-256 mismatch). "
                   "The download may be corrupted. Please try again later."));
            return;
        }
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

    // Launch MSI in silent upgrade mode via msiexec
    // /qb = basic UI (progress bar only)
    // /i  = install (MajorUpgrade in WiX handles the upgrade)
    QStringList args = {
        "/i", m_downloadedPath,
        "/qb",
        "REBOOT=ReallySuppress"   // don't reboot automatically
    };

    bool started = QProcess::startDetached("msiexec.exe", args);

    if (started) {
        emit updateLaunched();
        // The MainWindow should connect to this signal and call qApp->quit()
    } else {
        qWarning() << "UpdateChecker: failed to launch msiexec";
        // D5: Rollback — app continues running, user can try again
    }
}

} // namespace gp
