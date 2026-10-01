// SPDX-License-Identifier: Apache-2.0
// ── Vendored 7-Zip bundle + app-owned resolution pins ───────────────────────
// PARITY-SCORECARD-2026-09-30 §4 row 14 (July audit §3 row 75): the encrypted
// package feature used to depend on a system-installed 7z.exe (PATH, then the
// conventional Program Files locations), contradicting the offline pitch — a
// machine without 7-Zip lost AES-256 encrypted packages entirely. The fix
// commits the official 7-Zip 26.02 x64 binaries into the repo
// (third_party/7zip/bin/, pinned SHA-256, provenance + license recorded) and
// resolves them FIRST via the application-owned directory.
//
// The pins cover the four properties the deliverable stands on:
//   1. the bundle is committed and byte-exact against the recorded SHA-256
//      pins (a swapped/corrupted vendored binary fails the suite), with the
//      provenance + license discipline actually present;
//   2. resolution prefers the app-owned copy over any system install;
//   3. absence is disclosed honestly (empty result, never a bogus path);
//   4. the BUNDLED binary itself performs the real M-1 stdin-password
//      create/validate round trip (no system 7z involved via the override).
#include <QtTest/QtTest>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "engines/SafeSave.h"
#include "shell/controllers/HomeController.h"

using gp::HomeController;

namespace SafeSave = gp::SafeSave;

namespace {

constexpr char kPinned7zExeSha256[] =
    "83967f1b02b43c4efeda302795722c809e0e81b8307de73558d10484d5676a7d";
constexpr char kPinned7zDllSha256[] =
    "69fd4df057985c40e510e2fac182881c7f85e90aa13ec703f763a8fdb2ce61f8";

QString sha256OfFile(const QString& path) {
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

#ifndef SOURCE_DIR
#define SOURCE_DIR "."
#endif

QString bundleDir() {
    return QDir(SOURCE_DIR).filePath(QStringLiteral("third_party/7zip/bin"));
}

// Plant a resolution-qualifying (but fake) tool pair in a directory.
void plantFakeToolPair(const QString& dir) {
    QDir().mkpath(dir);
    for (const QString& name : { QStringLiteral("7z.exe"), QStringLiteral("7z.dll") }) {
        QFile f(dir + QLatin1Char('/') + name);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QVERIFY(f.write("not really 7z") > 0);
        f.close();
    }
}

} // namespace

class TestSevenZipBundle : public QObject {
    Q_OBJECT

private slots:
    // Pin 1 — the deliverable's license+hash discipline. The vendored binaries
    // must exist in the repo, match the pinned SHA-256 values exactly, and the
    // provenance + license records must be present and name those hashes (a
    // bundled binary without recorded license+hash is a FAILED deliverable).
    void vendoredBinariesAreCommittedAndHashPinned() {
        const QString exe = bundleDir() + QStringLiteral("/7z.exe");
        const QString dll = bundleDir() + QStringLiteral("/7z.dll");
        QVERIFY2(QFileInfo::exists(exe), "vendored 7z.exe missing from third_party/7zip/bin");
        QVERIFY2(QFileInfo::exists(dll), "vendored 7z.dll missing from third_party/7zip/bin");
        QCOMPARE(sha256OfFile(exe), QString::fromLatin1(kPinned7zExeSha256));
        QCOMPARE(sha256OfFile(dll), QString::fromLatin1(kPinned7zDllSha256));

        // Provenance: exact version, source URL, installer hash, extraction.
        const QString provenance =
            QDir(SOURCE_DIR).filePath(QStringLiteral("third_party/7zip/PROVENANCE.md"));
        QVERIFY2(QFileInfo::exists(provenance), "PROVENANCE.md missing");
        QFile prov(provenance);
        QVERIFY(prov.open(QIODevice::ReadOnly));
        const QString provText = QString::fromUtf8(prov.readAll());
        QVERIFY2(provText.contains(QStringLiteral("26.02")),
                 "provenance must record the exact version");
        QVERIFY2(provText.contains(QStringLiteral("7z2602-x64.exe")),
                 "provenance must record the source artifact");
        QVERIFY2(provText.contains(QLatin1String(kPinned7zExeSha256)),
                 "provenance must record the 7z.exe hash");
        QVERIFY2(provText.contains(QLatin1String(kPinned7zDllSha256)),
                 "provenance must record the 7z.dll hash");

        // License travels with the bundle (LGPL / BSD / unRAR disclosure).
        const QString license =
            QDir(SOURCE_DIR).filePath(QStringLiteral("third_party/7zip/License.txt"));
        QVERIFY2(QFileInfo::exists(license), "License.txt missing from the bundle");
        QFile lic(license);
        QVERIFY(lic.open(QIODevice::ReadOnly));
        const QString licText = QString::fromUtf8(lic.readAll());
        QVERIFY2(licText.contains(QStringLiteral("LGPL"), Qt::CaseInsensitive),
                 "license text must disclose the LGPL grant");
        QVERIFY2(licText.contains(QStringLiteral("unRAR"), Qt::CaseInsensitive),
                 "license text must disclose the unRAR restriction");
    }

    // Pin 2 — resolution order: the app-owned directory WINS over any system
    // installation. This is the property that removes the external-binary
    // dependency: an official install carries its own pinned copy.
    void resolverPrefersAppOwnedBinary() {
        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        plantFakeToolPair(appDir.path());

        const QString resolved =
            HomeController::locateSevenZip(appDir.path());
        QCOMPARE(resolved, QDir(appDir.path()).filePath(QStringLiteral("7z.exe")));
    }

    // Pin 3 — honest absence: a location without the tool pair must never be
    // reported as the resolved tool. The resolver either falls back to a real
    // system installation (existing file) or returns EMPTY — the caller turns
    // an empty result into the explicit "capability unavailable" disclosure.
    void resolverDisclosesAbsenceHonestly() {
        QTemporaryDir emptyDir;
        QVERIFY(emptyDir.isValid());

        const QString resolved = HomeController::locateSevenZip(emptyDir.path());
        QVERIFY2(resolved != QDir(emptyDir.path()).filePath(QStringLiteral("7z.exe")),
                 "an empty app directory must not be reported as resolved");
        QVERIFY2(resolved.isEmpty() || QFileInfo::exists(resolved),
                 "a non-empty resolution must point at an existing tool");
    }

    // Pin 4 — the bundled binary does the real work end-to-end through the
    // M-1 seams: password ONLY on stdin (never argv), create, encrypted
    // read-back, and rejection of a wrong password. Skips honestly when the
    // bundle itself is absent (negative-control leg proves pin 1 catches it).
    void bundledBinaryEndToEndStdinPassword() {
        const QString srcExe = bundleDir() + QStringLiteral("/7z.exe");
        const QString srcDll = bundleDir() + QStringLiteral("/7z.dll");
        if (!QFileInfo::exists(srcExe) || !QFileInfo::exists(srcDll))
            QSKIP("vendored 7-Zip bundle not present — pins 2/3 still ran");

        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        const QString appDirPath = appDir.path();
        QVERIFY(QFile::copy(srcExe, appDirPath + QStringLiteral("/7z.exe")));
        QVERIFY(QFile::copy(srcDll, appDirPath + QStringLiteral("/7z.dll")));

        const QString sevenZip = HomeController::locateSevenZip(appDirPath);
        QCOMPARE(sevenZip, QDir(appDirPath).filePath(QStringLiteral("7z.exe")));

        // The bundled binary IS the pinned 26.02 version.
        QProcess banner;
        banner.start(sevenZip, {});
        QVERIFY(banner.waitForStarted(10000));
        QVERIFY(banner.waitForFinished(30000));
        QCOMPARE(banner.exitCode(), 0);
        const QString bannerText = QString::fromUtf8(banner.readAllStandardOutput());
        QVERIFY2(bannerText.contains(QStringLiteral("7-Zip 26.02")),
                 qPrintable(QStringLiteral("bundled binary reports: %1").arg(bannerText.left(80))));

        // Real payload through the M-1 contract.
        QTemporaryDir work;
        QVERIFY(work.isValid());
        const QString payload = work.filePath(QStringLiteral("doc.txt"));
        {
            QFile f(payload);
            QVERIFY(f.open(QIODevice::WriteOnly));
            QVERIFY(f.write("vendored 7z package payload") > 0);
        }
        const QString archive = work.filePath(QStringLiteral("pkg.zip"));
        const QByteArray pwStdin = QByteArrayLiteral("V3ndored-Pass\n");

        bool canceled = false;
        int exitCode = -1;
        QString err;
        QVERIFY2(SafeSave::runBoundedProcess(
                     sevenZip,
                     HomeController::encryptedPackageCreateArgs(archive, payload),
                     60000, {}, &canceled, &exitCode, &err, pwStdin),
                 qPrintable(QStringLiteral("bundled 7z create failed: %1").arg(err)));
        QCOMPARE(exitCode, 0);
        QVERIFY(QFileInfo::exists(archive));

        // Encrypted read-back with the stdin password.
        exitCode = -1;
        QVERIFY2(SafeSave::runBoundedProcess(
                     sevenZip,
                     HomeController::encryptedPackageValidateArgs(archive),
                     60000, {}, &canceled, &exitCode, &err, pwStdin),
                 qPrintable(QStringLiteral("bundled 7z read-back failed: %1").arg(err)));
        QCOMPARE(exitCode, 0);

        // Negative: a wrong password must NOT open the archive.
        exitCode = -1;
        {
            const bool finished = SafeSave::runBoundedProcess(
                sevenZip,
                QStringList{ QStringLiteral("t"), QStringLiteral("-pWRONG"),
                             QDir::toNativeSeparators(archive) },
                15000, {}, &canceled, &exitCode, &err);
            QVERIFY2(!finished || exitCode != 0,
                     "a wrong password must not open the bundled-7z archive");
        }
    }
};

QTEST_GUILESS_MAIN(TestSevenZipBundle)
#include "TestSevenZipBundle.moc"
