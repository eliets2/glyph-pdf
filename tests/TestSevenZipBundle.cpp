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
// plus the r3-sec runtime-integrity property (security M, CWE-494):
//   5. the staged pair is RE-VERIFIED at resolution against the pins
//      compiled into THIS binary — the configure-time file(SHA256) gate
//      protects the build host, not the install directory, and the resolved
//      7z.exe receives the document bytes AND the package password. A
//      tampered/stale copy is REFUSED with the honest integrity disclosure;
//      a good copy passes; the verdict is cached per session.
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
#include "engines/SevenZipLocator.h"
// The M-1 argv-builder seams (encryptedPackageCreateArgs/ValidateArgs) live on
// HomeController; the locator seam under test is engines/SevenZipLocator (the
// dedicated owner of the vendored-bundle policy — extracted from SafeSave,
// which stays tool-agnostic).
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

// Plant byte-exact copies of the REAL committed bundle (r3-sec: resolution
// re-verifies the staged bytes, so resolution-order pins must stage
// integrity-valid bytes — the fake pair is now the tamper pin's fixture).
bool plantRealBundle(const QString& dir) {
    const QString srcExe = bundleDir() + QStringLiteral("/7z.exe");
    const QString srcDll = bundleDir() + QStringLiteral("/7z.dll");
    if (!QFileInfo::exists(srcExe) || !QFileInfo::exists(srcDll)) return false;
    QDir().mkpath(dir);
    if (!QFile::exists(dir + QStringLiteral("/7z.exe"))
        && !QFile::copy(srcExe, dir + QStringLiteral("/7z.exe"))) return false;
    if (!QFile::exists(dir + QStringLiteral("/7z.dll"))
        && !QFile::copy(srcDll, dir + QStringLiteral("/7z.dll"))) return false;
    return true;
}

// Deterministic tamper: flip one bit of a byte deep inside the file (past
// any header the loader would care about — the bytes stop matching the pin).
bool flipByte(const QString& path, qint64 offset) {
    QFile f(path);
    if (!f.open(QIODevice::ReadWrite)) return false;
    const QByteArray bytes = f.readAll();
    if (offset >= bytes.size()) return false;
    QByteArray tampered = bytes;
    tampered[offset] = static_cast<char>(bytes[offset] ^ 0x01);
    f.seek(0);
    return f.write(tampered) == tampered.size();
}

constexpr qint64 kTamperOffset = 0x200;

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
    // dependency: an official install carries its own pinned copy. (r3-sec:
    // resolution now RE-VERIFIES the staged bytes, so the preference proof
    // stages integrity-valid copies of the committed bundle — a fake pair
    // must be REFUSED, which is pin 5's subject.)
    void resolverPrefersAppOwnedBinary() {
        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        QVERIFY2(plantRealBundle(appDir.path()),
                 "committed bundle unavailable — pin 1 covers its presence");

        const QString resolved =
            gp::SevenZipLocator::locateForTesting(appDir.path());
        QCOMPARE(resolved,
                 QDir::toNativeSeparators(
                     QDir(appDir.path()).filePath(QStringLiteral("7z.exe"))));
    }

    // Pin 3 — honest absence: without the app-owned tool pair the resolver
    // returns EMPTY, full stop. The wave-2b security audit (F-02, CWE-427)
    // removed the PATH and Program-Files fallback legs: a planted 7z.exe there
    // would receive the document and the package password with no hash
    // verification, so absence must reach the caller's explicit
    // "capability unavailable" disclosure rather than any system tool.
    void resolverDisclosesAbsenceHonestly() {
        QTemporaryDir emptyDir;
        QVERIFY(emptyDir.isValid());

        const QString resolved = gp::SevenZipLocator::locateForTesting(emptyDir.path());
        QVERIFY2(resolved.isEmpty(),
                 "without the bundled pair the resolver must return EMPTY — "
                 "no PATH or Program-Files fallback may satisfy it");

        // Absence is NOT an integrity failure — the caller discloses the two
        // states differently, so the out-param must stay empty here.
        QString absenceErr = QStringLiteral("sentinel");
        QVERIFY(gp::SevenZipLocator::locateVerifiedForTesting(emptyDir.path(), &absenceErr).isEmpty());
        QVERIFY2(absenceErr.isEmpty(),
                 "plain absence must not be reported as an integrity failure");
    }

    // ── Pin 5 — r3-sec runtime integrity (security M, CWE-494) ─────────────
    //
    // The configure-time file(SHA256) gate protects the BUILD HOST only: after
    // staging/deploy nothing re-checked the bytes, yet the resolved 7z.exe
    // receives the document bytes AND the package password (the M-1 stdin
    // contract). The resolver must therefore re-hash BOTH staged files against
    // the pins compiled into THIS binary at resolution and refuse to launch a
    // tampered or stale copy with the honest integrity disclosure.

    // A tampered (byte-flipped) 7z.exe — and, symmetric leg, a tampered
    // 7z.dll — is REFUSED with the integrity message; junk bytes (never a
    // real 7z at all — the fake-pair fixture pin 2 used pre-r3sec) are
    // equally refused. Absence stays distinguishable from tamper (pin 3).
    void tamperedBundledPairIsRefusedWithIntegrityDisclosure() {
        // Tampered EXE leg: real bytes, one bit flipped.
        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        QVERIFY(plantRealBundle(appDir.path()));
        QVERIFY2(flipByte(appDir.path() + QStringLiteral("/7z.exe"), kTamperOffset),
                 "tamper fixture failed — pin premise broken");
        QString integrityError;
        const QString resolved =
            gp::SevenZipLocator::locateVerifiedForTesting(appDir.path(), &integrityError);
        QVERIFY2(resolved.isEmpty(),
                 "a tampered bundled 7z.exe must be REFUSED at resolution — "
                 "it would receive the document bytes and the package password");
        QVERIFY2(integrityError.contains(
                     QStringLiteral("failed its integrity check")),
                 qPrintable(QStringLiteral("disclosure missing the honest "
                                           "integrity message: %1")
                                .arg(integrityError)));

        // Tampered DLL leg: the launcher alone is not the attack surface —
        // the format engine is, and the pin covers BOTH files.
        QTemporaryDir appDirDll;
        QVERIFY(appDirDll.isValid());
        QVERIFY(plantRealBundle(appDirDll.path()));
        QVERIFY(flipByte(appDirDll.path() + QStringLiteral("/7z.dll"), kTamperOffset));
        QString dllErr;
        QVERIFY(gp::SevenZipLocator::locateVerifiedForTesting(appDirDll.path(), &dllErr).isEmpty());
        QVERIFY2(dllErr.contains(QStringLiteral("failed its integrity check")),
                 qPrintable(dllErr));

        // Junk-bytes leg: a staged pair that never was 7-Zip (stale garbage).
        QTemporaryDir fakeDir;
        QVERIFY(fakeDir.isValid());
        plantFakeToolPair(fakeDir.path());
        QString fakeErr;
        QVERIFY(gp::SevenZipLocator::locateVerifiedForTesting(fakeDir.path(), &fakeErr).isEmpty());
        QVERIFY2(fakeErr.contains(QStringLiteral("failed its integrity check")),
                 qPrintable(fakeErr));
    }

    // A GOOD copy — byte-exact against the pins — resolves normally and
    // carries no integrity error.
    void goodBundledPairPassesRuntimeVerification() {
        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        QVERIFY(plantRealBundle(appDir.path()));

        QString integrityError = QStringLiteral("sentinel");
        const QString resolved =
            gp::SevenZipLocator::locateVerifiedForTesting(appDir.path(), &integrityError);
        QCOMPARE(resolved,
                 QDir::toNativeSeparators(
                     QDir(appDir.path()).filePath(QStringLiteral("7z.exe"))));
        QVERIFY2(integrityError.isEmpty(),
                 qPrintable(QStringLiteral("a good pair must not carry an "
                                           "integrity error: %1")
                                .arg(integrityError)));
    }

    // The check runs ONCE per session: the first verdict for a resolved path
    // is cached, so a file that turns hostile AFTER a clean resolution cannot
    // silently flip the cached verdict — and equally, a clean resolution is
    // not re-paid with re-hashing on every package operation. The control leg
    // proves the same tamper IS detectable on a fresh path (cache miss).
    void integrityCheckRunsOncePerSession() {
        QTemporaryDir appDir;
        QVERIFY(appDir.isValid());
        QVERIFY(plantRealBundle(appDir.path()));
        // First use: verified clean, verdict cached.
        QVERIFY(!gp::SevenZipLocator::locateVerifiedForTesting(appDir.path()).isEmpty());

        // Control — cache MISS on a fresh path: the identical tamper is
        // refused, proving the detection works and the appDir verdict below
        // can only be the cached one.
        QTemporaryDir probeDir;
        QVERIFY(probeDir.isValid());
        QVERIFY(plantRealBundle(probeDir.path()));
        QVERIFY(flipByte(probeDir.path() + QStringLiteral("/7z.exe"), kTamperOffset));
        QString probeErr;
        QVERIFY2(gp::SevenZipLocator::locateVerifiedForTesting(probeDir.path(), &probeErr).isEmpty(),
                 "control failed: the tamper is not being detected at all");
        QVERIFY(!probeErr.isEmpty());

        // Now turn the ALREADY-VERIFIED path hostile: the cached verdict
        // holds for the session (no re-hash, no flip-flop).
        QVERIFY(flipByte(appDir.path() + QStringLiteral("/7z.exe"), kTamperOffset));
        QVERIFY2(!gp::SevenZipLocator::locateVerifiedForTesting(appDir.path()).isEmpty(),
                 "the first-use verdict must be cached per session — a second "
                 "resolution must not re-hash the staged files");
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

        const QString sevenZip = gp::SevenZipLocator::locateForTesting(appDirPath);
        QCOMPARE(sevenZip,
                 QDir::toNativeSeparators(
                     QDir(appDirPath).filePath(QStringLiteral("7z.exe"))));

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
