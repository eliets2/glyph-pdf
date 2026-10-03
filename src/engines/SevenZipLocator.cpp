// SPDX-License-Identifier: Apache-2.0
#include "engines/SevenZipLocator.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QtGlobal>

#ifndef GLYPHPDF_7ZIP_EXE_SHA256
// Non-staged trees (non-Windows configure) compile no pins; verification is
// then not enforceable and the build says so honestly (see below).
#define GLYPHPDF_7ZIP_EXE_SHA256 ""
#endif
#ifndef GLYPHPDF_7ZIP_DLL_SHA256
#define GLYPHPDF_7ZIP_DLL_SHA256 ""
#endif

namespace gp::SevenZipLocator {

// Contract: see the header note. Bundled-only since the wave-2b security
// audit (F-02, CWE-427); the body is verbatim from its previous home in
// SafeSave.cpp (r3-api lane extraction — SafeSave stays tool-agnostic).
QString locateForTesting(const QString& appDir)
{
    if (!appDir.isEmpty()) {
        const QString bundled = appDir + QStringLiteral("/7z.exe");
        if (QFileInfo::exists(bundled)
            && QFileInfo::exists(appDir + QStringLiteral("/7z.dll")))
            return QDir::toNativeSeparators(bundled);
    }
    return {};
}

QString locate()
{
    return locateForTesting(QCoreApplication::applicationDirPath());
}

namespace {

// Hex SHA-256 of a file's bytes; empty when the file cannot be fully read
// (absent, unreadable mid-read — an unreadable staged binary is a failure,
// not a pass).
QString fileSha256Hex(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buf[65536];
    while (!f.atEnd()) {
        const qint64 n = f.read(buf, sizeof(buf));
        if (n <= 0) return {};
        hash.addData(QByteArrayView(buf, static_cast<int>(n)));
    }
    return QString::fromLatin1(hash.result().toHex());
}

struct SevenZipVerdict {
    bool ok = false;
    QString error;  // user-presentable when !ok
};

QMutex g_sevenZipVerifyMutex;
QHash<QString, SevenZipVerdict> g_sevenZipVerifyCache;  // native exe path -> verdict

// Re-hash the staged pair against the pins compiled into THIS binary.
bool stagedSevenZipMatchesPins(const QString& appDir, SevenZipVerdict* verdict)
{
    constexpr const char* kPinnedExe = GLYPHPDF_7ZIP_EXE_SHA256;
    constexpr const char* kPinnedDll = GLYPHPDF_7ZIP_DLL_SHA256;
    if (qstrlen(kPinnedExe) == 0 || qstrlen(kPinnedDll) == 0) {
        // Unpinned build (configure found no bundle to stage — dev/non-Windows
        // trees): there is nothing to compare against. Say so honestly and
        // keep the historical behavior; shipped Windows builds always pin
        // (the CMake gate FATALs without the bundle).
        qWarning() << "SevenZipLocator: this build carries no compiled-in 7-Zip "
                      "SHA-256 pins (non-staged tree) — the staged bundle "
                      "cannot be re-verified at runtime";
        return true;
    }
    const struct { const char* what; const char* pin; } checks[] = {
        { "7z.exe", kPinnedExe },
        { "7z.dll", kPinnedDll },
    };
    for (const auto& check : checks) {
        const QString actual =
            fileSha256Hex(appDir + QLatin1Char('/') + QLatin1String(check.what));
        if (actual.isEmpty() || actual != QLatin1String(check.pin)) {
            verdict->error = QObject::tr(
                "Encrypted packaging is unavailable: the 7-Zip tool bundled "
                "with GlyphPDF (7z.exe/7z.dll beside the application) failed "
                "its integrity check — the SHA-256 of the staged %1 does "
                "not match the value pinned for this build. The bundled tool "
                "will not be launched with your document or your package "
                "password. Reinstall GlyphPDF to restore encrypted packages, "
                "or use Protect â¸ Encrypt to password-protect the "
                "PDF directly.").arg(QLatin1String(check.what));
            return false;
        }
    }
    return true;
}

} // namespace

QString locateVerifiedForTesting(const QString& appDir, QString* integrityError)
{
    return locateVerifiedIn(appDir, integrityError);
}

QString locateVerified(QString* integrityError)
{
    return locateVerifiedIn(QCoreApplication::applicationDirPath(), integrityError);
}

QString locateVerifiedIn(const QString& appDir, QString* integrityError)
{
    if (integrityError) integrityError->clear();
    if (appDir.isEmpty()) return {};
    const QString bundled = appDir + QStringLiteral("/7z.exe");
    if (!(QFileInfo::exists(bundled)
          && QFileInfo::exists(appDir + QStringLiteral("/7z.dll"))))
        return {};
    const QString native = QDir::toNativeSeparators(bundled);

    // Per-session verdict cache (first use per resolved path).
    {
        QMutexLocker lock(&g_sevenZipVerifyMutex);
        const auto it = g_sevenZipVerifyCache.constFind(native);
        if (it != g_sevenZipVerifyCache.constEnd()) {
            if (it->ok) return native;
            if (integrityError) *integrityError = it->error;
            return {};
        }
    }
    SevenZipVerdict verdict;
    verdict.ok = stagedSevenZipMatchesPins(appDir, &verdict);
    if (!verdict.ok) {
        qWarning() << "SevenZipLocator: refusing to launch the bundled 7-Zip — "
                      "it failed its integrity check (7z.exe/7z.dll beside"
                   << appDir << ")";
    }
    {
        QMutexLocker lock(&g_sevenZipVerifyMutex);
        g_sevenZipVerifyCache.insert(native, verdict);
    }
    if (!verdict.ok) {
        if (integrityError) *integrityError = verdict.error;
        return {};
    }
    return native;
}

} // namespace gp::SevenZipLocator
