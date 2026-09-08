// SPDX-License-Identifier: Apache-2.0
// INF01 (P1) regression suite — the standalone cleanup CLI.
//
// INF01 (TEAM-INFRASTRUCTURE-REVIEW-2026-09-07, tools/clean_scanned_pdf.py
// main()): the tool unlinked the requested output PDF and the stale
// page_*_cleaned.png artifacts BEFORE opening the input. Consequences:
//   - same input/output deletes the source, then fails to open it,
//   - an invalid input destroys an existing destination before reporting the
//     input failure,
//   - any rendering/processing error after that point leaves the previous
//     output lost.
//
// Required repair: open/validate the input FIRST, write to a sibling
// candidate, validate/close it, then replace the output atomically; reject
// same-file aliases; preserve prior output on every failure; stale PNG
// artifacts are only cleared after a successful commit.
//
// These tests drive the real script end-to-end via python (fixtures are
// generated with QPdfWriter). The processing-failure case uses the script's
// documented CLEAN_SCANNED_PDF_FAIL_AFTER_PAGE test seam. Skips honestly when
// python3 with PyMuPDF/OpenCV/numpy/Pillow is unavailable (mirrors the
// model-dependent skip conventions).
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QProcess>
#include <QDir>
#include <QPdfWriter>
#include <QPainter>
#include <QSettings>
#include <QRegularExpression>
#include <QStandardPaths>

#ifndef CLEANUP_CLI_SOURCE_DIR
#define CLEANUP_CLI_SOURCE_DIR "."
#endif

class TestCleanupCli : public QObject {
    Q_OBJECT
private:
    QString m_scriptPath;
    bool m_pythonOk = false;
    QString m_pythonExe;
    QProcessEnvironment m_childEnv;

    // msys2 login shells (the documented build/test entry) wipe USERPROFILE
    // and APPDATA; a Windows python then cannot resolve its USER site
    // packages (where PyMuPDF et al. are usually pip-installed). Recover the
    // session values from the registry so child interpreters see a normal
    // Windows environment.
    static QString registryUserEnv(const QString& name) {
        QProcess proc;
        proc.start(QStringLiteral("reg"),
                   QStringList{ QStringLiteral("query"),
                                QStringLiteral("HKCU\\Volatile Environment"),
                                QStringLiteral("/v"), name });
        if (!proc.waitForFinished(10000) || proc.exitCode() != 0) return {};
        const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
        const QRegularExpression re(
            QRegularExpression::escape(name)
            + QStringLiteral("\\s+REG_SZ\\s+(\\S+)"));
        const auto m = re.match(out);
        return m.hasMatch() ? m.captured(1) : QString();
    }

    QProcessEnvironment pythonEnv() const {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString userProfile = registryUserEnv(QStringLiteral("USERPROFILE"));
        const QString appData = registryUserEnv(QStringLiteral("APPDATA"));
        if (!userProfile.isEmpty())
            env.insert(QStringLiteral("USERPROFILE"), userProfile);
        if (!appData.isEmpty())
            env.insert(QStringLiteral("APPDATA"), appData);
        return env;
    }

    static QString makeMultiPagePdf(const QString& dir, const QString& name, int pages) {
        const QString path = dir + "/" + name;
        QPdfWriter writer(path);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setResolution(72);
        QPainter p(&writer);
        for (int i = 0; i < pages; ++i) {
            if (i > 0) writer.newPage();
            // Enough ink that the cleanup pipeline does not classify the page
            // as blank (it skips blank pages; a zero-page output would abort).
            p.fillRect(QRect(60, 60, 240, 240), Qt::black);
            p.drawText(80, 380, QStringLiteral("INF01 fixture page %1").arg(i + 1));
        }
        p.end();
        return path;
    }

    static QByteArray sha256(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&f);
        return hash.result();
    }

    static bool writeFile(const QString& path, const QByteArray& bytes) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        return f.write(bytes) == bytes.size();
    }

    // Stale artifact from a previous run the failing path must NOT destroy.
    static bool makeStalePng(const QString& pngDir, const QString& name) {
        QDir().mkpath(pngDir);
        // A real 1x1 PNG (enough for unlink/preservation semantics).
        static const QByteArray png = QByteArray::fromHex(
            "89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c489"
            "0000000d4944415478da63fcffff3f030005fe02fea72d1e480000000049454e44"
            "ae426082");
        return writeFile(pngDir + "/" + name, png);
    }

    struct RunResult {
        int exitCode = -1;
        QByteArray stdoutErr;
    };

    RunResult run(const QStringList& args) const {
        RunResult r;
        QProcess proc;
        proc.setProcessEnvironment(m_childEnv);
        proc.setWorkingDirectory(QFileInfo(m_scriptPath).absolutePath());
        proc.start(m_pythonExe, QStringList{ m_scriptPath } << args);
        const bool started = proc.waitForStarted(15000);
        if (started) {
            proc.waitForFinished(300000);
            r.exitCode = proc.exitCode();
            r.stdoutErr = proc.readAllStandardOutput() + proc.readAllStandardError();
        } else {
            r.stdoutErr = "process failed to start";
        }
        return r;
    }

private slots:
    void initTestCase() {
        m_scriptPath = QStringLiteral(CLEANUP_CLI_SOURCE_DIR)
                           + QStringLiteral("/tools/clean_scanned_pdf.py");
        if (!QFileInfo::exists(m_scriptPath)) {
            qWarning() << "cleanup script not found at" << m_scriptPath;
            QSKIP("cleanup script not found");
        }
        qWarning() << "env USERPROFILE=" << qEnvironmentVariable("USERPROFILE");
        m_childEnv = pythonEnv();
        if (m_childEnv.value(QStringLiteral("USERPROFILE")).isEmpty()
            || m_childEnv.value(QStringLiteral("APPDATA")).isEmpty()) {
            qWarning() << "registry env recovery incomplete: USERPROFILE="
                       << m_childEnv.value(QStringLiteral("USERPROFILE"))
                       << "APPDATA=" << m_childEnv.value(QStringLiteral("APPDATA"));
        }

        // The script needs python3 with fitz (PyMuPDF), cv2, numpy and PIL.
        // "python" may resolve to a distribution without them (msys2, Windows
        // Store stub), so walk the usual candidates and pick the first that
        // can import the stack; skip honestly when none can.
        const QString localAppData = m_childEnv.value(
            QStringLiteral("LOCALAPPDATA"));
        const QStringList candidates = {
            QStringLiteral("python"),
            QStringLiteral("python3"),
            QStringLiteral("C:/Python314/python.exe"),
            QStringLiteral("C:/Python313/python.exe"),
            QStringLiteral("C:/Python312/python.exe"),
            localAppData + QStringLiteral("/Programs/Python/Python314/python.exe"),
            localAppData + QStringLiteral("/Programs/Python/Python313/python.exe"),
            localAppData + QStringLiteral("/Programs/Python/Python312/python.exe"),
        };
        QStringList tried;
        for (const QString& exe : candidates) {
            if (exe.isEmpty() || !QStandardPaths::findExecutable(exe).isEmpty()
                || QFile::exists(exe)) {
                QProcess probe;
                probe.setProcessEnvironment(m_childEnv);
                probe.start(exe,
                            QStringList{ QStringLiteral("-c"),
                                         QStringLiteral("import fitz, cv2, numpy, PIL") });
                tried << exe;
                if (probe.waitForFinished(60000) && probe.exitCode() == 0) {
                    m_pythonExe = exe;
                    break;
                }
                qWarning() << "probe failed for" << exe << "exit" << probe.exitCode()
                           << ":" << probe.readAllStandardError().constData()
                           << "| restored USERPROFILE="
                           << m_childEnv.value(QStringLiteral("USERPROFILE"))
                           << "APPDATA=" << m_childEnv.value(QStringLiteral("APPDATA"));
            }
        }
        if (m_pythonExe.isEmpty()) {
            qWarning() << "probed interpreters:" << tried.join(QStringLiteral(", "));
            QSKIP("python3 with PyMuPDF/OpenCV/numpy/Pillow not available");
        }
        m_pythonOk = true;
    }

    // THE INF01 reproduction: same input/output used to delete the source
    // before failing to open it. Now: explicit rejection, source untouched.
    void sameInputOutputRejectedPreservesSource();
    // Nonexistent input must not destroy an existing destination.
    void nonexistentInputPreservesExistingOutput();
    // Corrupt input must not destroy an existing destination.
    void corruptInputPreservesExistingOutput();
    // Deterministic mid-processing failure must preserve the source, the
    // pre-existing output and untouched pre-existing artifacts, and leave no
    // candidate file behind.
    void processingFailurePreservesSourceAndOutput();
    // A successful run still produces the cleaned output, report and PNGs,
    // replaces a pre-existing destination, and clears stale artifacts it did
    // not rewrite (control: passes before and after the repair).
    void successReplacesOutputAndClearsStaleArtifacts();
};

void TestCleanupCli::sameInputOutputRejectedPreservesSource() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeMultiPagePdf(tmp.path(), QStringLiteral("same.pdf"), 2);
    QVERIFY(QFile::exists(pdf));
    const QByteArray sha = sha256(pdf);

    const RunResult r = run({ pdf, tmp.path(), pdf });

    QVERIFY2(r.exitCode != 0, "same input/output must be rejected");
    QVERIFY2(QFile::exists(pdf), "source must survive a same-file invocation");
    QCOMPARE(sha256(pdf), sha);
    QVERIFY2(r.stdoutErr.contains("same"),
             "rejection must say why (input/output alias)");
}

void TestCleanupCli::nonexistentInputPreservesExistingOutput() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString input = tmp.path() + QStringLiteral("/missing.pdf");
    const QString outDir = tmp.path() + QStringLiteral("/out");
    QDir().mkpath(outDir);
    const QString output = outDir + QStringLiteral("/prev.pdf");
    const QByteArray prev = QByteArray("%PDF-1.4 previous output bytes\n");
    QVERIFY(writeFile(output, prev));

    const RunResult r = run({ input, outDir, output });

    QVERIFY2(r.exitCode != 0, "missing input must be reported as failure");
    QCOMPARE(QFileInfo(output).exists(), true);
    QCOMPARE(sha256(output), QCryptographicHash::hash(prev, QCryptographicHash::Sha256));
}

void TestCleanupCli::corruptInputPreservesExistingOutput() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString input = tmp.path() + QStringLiteral("/corrupt.pdf");
    QVERIFY(writeFile(input, QByteArray("this is not a pdf\n")));
    const QString outDir = tmp.path() + QStringLiteral("/out");
    QDir().mkpath(outDir);
    const QString output = outDir + QStringLiteral("/prev.pdf");
    const QByteArray prev = QByteArray("%PDF-1.4 previous output bytes\n");
    QVERIFY(writeFile(output, prev));
    QVERIFY(makeStalePng(outDir + QStringLiteral("/png"),
                         QStringLiteral("page_009_cleaned.png")));

    const RunResult r = run({ input, outDir, output });

    QVERIFY2(r.exitCode != 0, "corrupt input must be reported as failure");
    QCOMPARE(sha256(output), QCryptographicHash::hash(prev, QCryptographicHash::Sha256));
    QVERIFY2(QFile::exists(outDir + QStringLiteral("/png/page_009_cleaned.png")),
             "pre-existing artifact must survive an input failure");
}

void TestCleanupCli::processingFailurePreservesSourceAndOutput() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString input = makeMultiPagePdf(tmp.path(), QStringLiteral("proc.pdf"), 3);
    QVERIFY(QFile::exists(input));
    const QByteArray srcSha = sha256(input);

    const QString outDir = tmp.path() + QStringLiteral("/out");
    QDir().mkpath(outDir);
    const QString output = outDir + QStringLiteral("/prev.pdf");
    const QByteArray prev = QByteArray("%PDF-1.4 previous output bytes\n");
    QVERIFY(writeFile(output, prev));
    // Artifacts this run never reaches must survive the failure.
    QVERIFY(makeStalePng(outDir + QStringLiteral("/png"),
                         QStringLiteral("page_009_cleaned.png")));

    QProcessEnvironment env = m_childEnv;
    env.insert(QStringLiteral("CLEAN_SCANNED_PDF_FAIL_AFTER_PAGE"), QStringLiteral("1"));

    RunResult r;
    {
        QProcess proc;
        proc.setProcessEnvironment(env);
        proc.setWorkingDirectory(QFileInfo(m_scriptPath).absolutePath());
        proc.start(m_pythonExe,
                   QStringList{ m_scriptPath } << QStringList{ input, outDir, output });
        QVERIFY(proc.waitForStarted(15000));
        proc.waitForFinished(300000);
        r.exitCode = proc.exitCode();
        r.stdoutErr = proc.readAllStandardOutput() + proc.readAllStandardError();
    }

    QVERIFY2(r.exitCode != 0, "injected processing failure must be reported");
    QCOMPARE(sha256(input), srcSha);
    QCOMPARE(sha256(output), QCryptographicHash::hash(prev, QCryptographicHash::Sha256));
    QVERIFY2(QFile::exists(outDir + QStringLiteral("/png/page_009_cleaned.png")),
             "unreached pre-existing artifact must survive a processing failure");
    // No candidate/temporary output may be left behind.
    const QStringList leftovers = QDir(outDir).entryList(
        QStringList() << QStringLiteral("*cleaning-tmp*"), QDir::Files);
    QCOMPARE(leftovers, QStringList());
}

void TestCleanupCli::successReplacesOutputAndClearsStaleArtifacts() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString input = makeMultiPagePdf(tmp.path(), QStringLiteral("ok.pdf"), 2);
    QVERIFY(QFile::exists(input));

    const QString outDir = tmp.path() + QStringLiteral("/out");
    QDir().mkpath(outDir);
    const QString output = outDir + QStringLiteral("/out.pdf");
    QVERIFY(writeFile(output, QByteArray("old output to be replaced\n")));
    QVERIFY(makeStalePng(outDir + QStringLiteral("/png"),
                         QStringLiteral("page_009_cleaned.png")));

    const RunResult r = run({ input, outDir, output });

    QCOMPARE(r.exitCode, 0);
    QFile out(output);
    QVERIFY2(out.open(QIODevice::ReadOnly), "output must exist after success");
    QVERIFY2(out.read(4) == QByteArray("%PDF"), "output must be a PDF");
    out.close();
    QVERIFY2(QFileInfo(outDir + QStringLiteral("/cleanup_report.txt")).exists(),
             "cleanup report must be written");
    // Fresh artifact for the processed page exists; the stale unreached one
    // was cleared by the post-commit sweep.
    QVERIFY2(QFile::exists(outDir + QStringLiteral("/png/page_002_cleaned.png")),
             "cleaned page artifact must be written");
    QVERIFY2(!QFile::exists(outDir + QStringLiteral("/png/page_009_cleaned.png")),
             "stale unreached artifact must be cleared after a successful commit");
    QVERIFY2(QDir(outDir).entryList(QStringList() << QStringLiteral("*cleaning-tmp*"),
                                    QDir::Files).isEmpty(),
             "no candidate may be left behind");
}

QTEST_MAIN(TestCleanupCli)
#include "TestCleanupCli.moc"
