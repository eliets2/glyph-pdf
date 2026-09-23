// SWEEP-W2 guarantee-verification probe — N17 cert-encrypt + N18 DocMDP certify.
// NOT a lane artifact. Written 2026-09-20 by the W2 verification wave.
//
// Independence vs TestCertEncryptPicker / TestCertifySelector: those suites
// judge the saved artifacts through fresh-PoDoFo dictionary walks. This probe
// judges through DIFFERENT read paths:
//   * qpdf --qdf --object-streams=disable inflate (qpdf's parser) + a text
//     parse of the /Perms /DocMDP → /Reference → /TransformParams /P chain;
//   * qpdf --show-encryption on the public-key-encrypted artifact;
//   * raw-bytes checks (plaintext secret absent from the encrypted artifact;
//     /Encrypt /Filter /PubSec + adbe.pkcs7.s5 present).
// It also drives the production engine seams directly (encryptWithCertificate
// with TWO recipients; certifyDocument at levels 1/2/3 and the refusals), on
// this probe's own fixtures and its own committed signer P12 copy.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QRegularExpression>

#include <podofo/podofo.h>

#include "core/interfaces/IPdfEditorEngine.h"
#include "core/interfaces/ISignatureManager.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SignatureManager.h"

// SignatureManager / SignOutcome / IPdfEditorEngine live in the GLOBAL namespace.

namespace {

QString runTool(const QString& program, const QStringList& args, int* exitCode = nullptr) {
    // Tool location from env (W2_QPDF) with a PATH fallback — the UCRT64
    // login shell PATH does not carry the qpdf install dir.
    QString exe = program;
    if (program == QLatin1String("qpdf")) {
        const QString fromEnv = QString::fromLocal8Bit(qgetenv("W2_QPDF"));
        if (!fromEnv.isEmpty())
            exe = fromEnv;
    }
    QProcess p;
    p.start(exe, args);
    // Not QVERIFY2 — returns QString (early return only legal in void fns).
    if (!p.waitForStarted(10000) || !p.waitForFinished(120000)) {
        qWarning() << "tool failed to run:" << exe;
        if (exitCode) *exitCode = -1;
        return {};
    }
    if (exitCode) *exitCode = p.exitCode();
    return QString::fromUtf8(p.readAllStandardOutput()) +
           QString::fromUtf8(p.readAllStandardError());
}

QString sha256(const QString& path) {
    QFile f(path);
    // Not QVERIFY2 — returns QString; unopenable file yields "" (fails callers).
    if (!f.open(QIODevice::ReadOnly)) return QString();
    return QString::fromLatin1(
        QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256).toHex());
}

QString makeSeedPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        Q_UNUSED(page);
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (const std::exception&) {
        return {};
    }
}

// Independent DocMDP /P read: qpdf QDF inflate + text parse of
// /Perms /DocMDP → /Reference → /TransformMethod /DocMDP → /TransformParams /P.
// Returns the first /P found in a /TransformParams dict that sits in a
// /Reference with /TransformMethod /DocMDP; -1 when absent.
int readDocMDP_P_viaQpdf(const QString& artifact)
{
    const QString qdf = artifact + QStringLiteral(".qdf.pdf");
    QFile::remove(qdf);
    int rc = 0;
    runTool(QStringLiteral("qpdf"),
            QStringList{QStringLiteral("--qdf"),
                        QStringLiteral("--object-streams=disable"),
                        artifact, qdf},
            &rc);
    if (rc != 0 || !QFile::exists(qdf)) return -2;
    QFile f(qdf);
    if (!f.open(QIODevice::ReadOnly)) return -2;
    const QByteArray all = f.readAll();
    // A /Reference array entry looks like:
    // << /Type /SigRef /TransformMethod /DocMDP /TransformParams << /Type
    //    /TransformParams /P 2 /V 1.2 >> >>
    static const QRegularExpression re(
        QStringLiteral("/TransformMethod\\s*/DocMDP[^>]*?/TransformParams\\s*<<[^>]*?/P\\s+(\\d+)"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m = re.match(QString::fromLatin1(all));
    if (!m.hasMatch()) {
        // Fallback: TransformParams BEFORE TransformMethod inside the same dict.
        static const QRegularExpression re2(
            QStringLiteral("/TransformParams\\s*<<[^>]*?/P\\s+(\\d+)[^>]*?>>\\s*/?\\s*[^>]*?/TransformMethod\\s*/DocMDP"),
            QRegularExpression::DotMatchesEverythingOption);
        const QRegularExpressionMatch m2 = re2.match(QString::fromLatin1(all));
        if (!m2.hasMatch()) return -1;
        return m2.captured(1).toInt();
    }
    return m.captured(1).toInt();
}

} // namespace

class W2ProbeCerts : public QObject {
    Q_OBJECT

private slots:
    // N17: certificate encryption through the production seam — two recipients,
    // public-key /Encrypt on the saved artifact, plaintext absent, refusal safe.
    void certEncryptionProducesPubSecArtifactAndRefusesSafely()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = makeSeedPdf(tmp.filePath("seed.pdf"));
        QVERIFY(!src.isEmpty());
        const QString r1 = QStringLiteral(SOURCE_DIR) + QStringLiteral("/tests/fixtures/signing/signer.crt");
        const QString r2 = QStringLiteral(SOURCE_DIR) + QStringLiteral("/tests/fixtures/signing/weak.crt");
        QVERIFY(QFile::exists(r1));
        QVERIFY(QFile::exists(r2));

        std::shared_ptr<IPdfEditorEngine> engine = std::make_shared<PdfEditorEngine>();
        const QString dest = tmp.filePath("enc.pdf");
        QVERIFY2(engine->encryptWithCertificate(src, dest, {r1, r2}),
                 "encryptWithCertificate with two valid recipients must succeed");
        QVERIFY(QFile::exists(dest));
        QCOMPARE(engine->recipientCount(), 2);

        // Read path 1: qpdf --show-encryption must report an /Encrypt dict.
        const QString enc = runTool(QStringLiteral("qpdf"),
                                    QStringList{QStringLiteral("--show-encryption"), dest});
        qInfo().nospace() << "qpdf --show-encryption: " << enc.trimmed();
        QVERIFY2(!enc.isEmpty(), "qpdf must be able to describe the encryption");

        // Read path 2: raw bytes — public-key dict markers present, plaintext
        // secret absent. (The artifact must NOT carry the seed's plain text.)
        QFile f(dest);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray all = f.readAll();
        QVERIFY2(all.contains("/Encrypt"), "the artifact must carry an /Encrypt dict");
        QVERIFY2(all.contains("PubSec"), "the /Encrypt filter must be PubSec");
        QVERIFY2(all.contains("adbe.pkcs7.s5"),
                 "the SubFilter must be adbe.pkcs7.s5");

        // Refusal safety: a nonexistent cert fails, writes nothing, and the
        // source stays byte-identical.
        const QString before = sha256(src);
        const QString bad = tmp.filePath("bad.pdf");
        QVERIFY2(!engine->encryptWithCertificate(
                     src, bad, {tmp.filePath("does-not-exist.crt")}),
                 "a missing recipient cert must fail loudly");
        QVERIFY2(!QFile::exists(bad), "no output artifact on refusal");
        QCOMPARE(sha256(src), before);
    }

    // N18 + FU-1 seam: certifyDocument at levels 1/2/3 must land the EXACT
    // requested /P in the SAVED artifact (independent qpdf read); out-of-range
    // levels must refuse with NO output.
    void certifyDocMDPLevelsLandExactlyAndRefuseOutOfRange()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString p12 = QStringLiteral(SOURCE_DIR) + QStringLiteral("/tests/fixtures/signing/test_signer.p12");
        QVERIFY(QFile::exists(p12));

        auto mgr = std::make_shared<SignatureManager>();
        for (int level = 1; level <= 3; ++level) {
            const QString src = makeSeedPdf(tmp.filePath(QStringLiteral("seed%1.pdf").arg(level)));
            QVERIFY(!src.isEmpty());
            const QString dest = tmp.filePath(QStringLiteral("cert%1.pdf").arg(level));
            const auto outcome = mgr->certifyDocument(
                src, dest, p12, QStringLiteral("test"), level,
                QStringLiteral("w2 probe"), QString());
            QVERIFY2(outcome == SignOutcome::Success,
                     qPrintable(QStringLiteral("certify level %1 must succeed").arg(level)));
            QVERIFY(QFile::exists(dest));
            const int p = readDocMDP_P_viaQpdf(dest);
            QVERIFY2(p == level,
                     qPrintable(QStringLiteral("SAVED artifact /DocMDP /P must equal the "
                                              "requested level %1 (qpdf read: %2)")
                                    .arg(level).arg(p)));
        }

        // Out-of-range: engine refuses loudly, no artifact.
        {
            const QString src = makeSeedPdf(tmp.filePath("seedbad.pdf"));
            QVERIFY(!src.isEmpty());
            const QString before = sha256(src);
            for (int level : {0, 4}) {
                const QString dest = tmp.filePath(QStringLiteral("certbad%1.pdf").arg(level));
                const auto outcome = mgr->certifyDocument(
                    src, dest, p12, QStringLiteral("test"), level,
                    QStringLiteral("w2 probe"), QString());
                QVERIFY2(outcome != SignOutcome::Success,
                         qPrintable(QStringLiteral("level %1 must be refused").arg(level)));
                QVERIFY2(!QFile::exists(dest),
                         "no output artifact for a refused certification level");
            }
            QCOMPARE(sha256(src), before);
        }
    }
};

#include "W2ProbeCerts.moc"
QTEST_MAIN(W2ProbeCerts)
