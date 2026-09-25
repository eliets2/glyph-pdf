// SPDX-License-Identifier: MIT
// W2BProbeSummaryPolicy.cpp — SWEEP-W2B INDEPENDENT verifier probe
//                            (W1-04, W1-05, F4, F6).
//
// Written by the W2B guarantee-verification-engine against the SAVED tip
// (feat/sweep-w2-verify-b @ 2d29a16). Independent hostile strings (different
// codepoints and mixtures than the fix lane's repros). Asserts the FIXED
// contracts:
//
//   W1-04: one hostile annotation string can no longer abort the printable
//     summary export — writePrintable succeeds, the SAVED artifact reopens as
//     a PDF through an independent read, non-WinAnsi content is shown as '?'
//     WITH an in-artifact disclosure note, and WinAnsi-representable text
//     (Latin-1, CP1252 specials) survives verbatim.
//   W1-05: the machine-policy trust model is disclosed WHEREVER overrides
//     render (statusLine) and in the support bundle (policy.trustModel).
//   F4: the TSA network-touchpoint row derives its enabled state from the
//     policy-EFFECTIVE value (enabled = will fire under current settings).
//   F6: the bundle privacyNote no longer overclaims "no file paths" — it
//     discloses the one machine-policy path exception.
//
// NC bases: W1-04 → 0441192^   W1-05 → b5dc14e^   F4 → 69fc6fb^   F6 → 20bdaa4^

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>
#include <memory>

#include "core/PolicyController.h"
#include "core/SupportBundle.h"
#include "core/AnnotationTypes.h"
#include "core/NetworkTouchpoints.h"
#include "engines/ReviewSummaryWriter.h"
#include "engines/pdfium/PdfiumBackend.h"
#include <podofo/podofo.h>

namespace {

QJsonObject squatterPolicy(const QString& tsaUrl)
{
    QJsonObject settings;
    if (!tsaUrl.isEmpty())
        settings.insert(QStringLiteral("signing/tsaUrl"), tsaUrl);
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    root.insert(QStringLiteral("settings"), settings);
    return root;
}

QString writePolicy(const QTemporaryDir& tmp, const QJsonObject& root)
{
    const QString path = tmp.filePath(QStringLiteral("policy.json"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();
    f.write(QJsonDocument(root).toJson());
    f.close();
    return path;
}

// Hostile string matrix: TAB, C0 control, CJK, emoji (surrogate pair), NUL.
QList<AnnotationItem> hostileComments()
{
    QList<AnnotationItem> items;
    AnnotationItem a;
    a.pageIndex = 0;
    a.mode = ToolMode::DrawRectangle;
    a.rect = QRectF(60, 60, 200, 40);
    a.author = QStringLiteral("Reviewer\u4e2d\u6587");           // CJK author
    a.text = QStringLiteral("needs\u4e2d\u6587 work \U0001F512 tab\there\x01");
    items.append(a);
    AnnotationItem b;
    b.pageIndex = 0;
    b.mode = ToolMode::DrawRectangle;
    b.rect = QRectF(60, 120, 200, 40);
    b.author = QStringLiteral("plain");
    b.text = QString::fromLatin1("nul\0after", 9);                    // embedded NUL
    items.append(b);
    return items;
}

QList<AnnotationItem> benignComments()
{
    QList<AnnotationItem> items;
    AnnotationItem a;
    a.pageIndex = 0;
    a.mode = ToolMode::DrawRectangle;
    a.rect = QRectF(60, 60, 200, 40);
    a.author = QStringLiteral("Ren\u00e9");                      // Latin-1 é
    a.text = QStringLiteral("co\u00f6rdinate \u2014 agree \u20ac100"); // ö, em-dash, €
    items.append(a);
    return items;
}

// Independent read path: extract the text layer of the SAVED artifact through
// PDFium — a SECOND engine, sharing nothing with the PoDoFo-based writer.
QString extractedText(const QString& path)
{
    PdfiumBackend backend;
    if (!backend.loadDocument(path))
        return QString();
    QString all;
    for (int p = 0; p < backend.pageCount(); ++p)
        all += backend.extractText(p) + QLatin1Char('\n');
    return all;
}

} // namespace

class W2BProbeSummaryPolicy : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> newTmp(const char *tag)
    {
        for (int attempt = 0; attempt < 2; ++attempt) {
            auto d = std::make_unique<QTemporaryDir>(
                QDir::tempPath() + QStringLiteral("/w2bprobe-%1-XXXXXX").arg(tag));
            if (d->isValid()) return d;
        }
        return nullptr;
    }

private slots:
    // ── W1-04: a hostile string degrades, the file survives ────────────────
    void hostileCommentNoLongerKillsTheExport()
    {
        auto tmp = newTmp("w104");
        QVERIFY(tmp);
        const QString out = tmp->filePath(QStringLiteral("summary.pdf"));
        QString err;
        QList<AnnotationItem> items = hostileComments();
        const bool ok = ReviewSummaryWriter::writePrintable(
            out, QStringLiteral("W2B Hostile Summary"), items,
            ReviewSummaryWriter::PrintOptions{}, &err);
        QVERIFY2(ok, qPrintable(QStringLiteral(
            "W1-04 REGRESSION: one hostile comment (TAB/CJK/emoji/control) "
            "aborted the ENTIRE export again: %1").arg(err)));
        QVERIFY(QFileInfo(out).exists() && QFileInfo(out).size() > 0);

        // The SAVED artifact is a real PDF (independent read).
        try {
            PoDoFo::PdfMemDocument d;
            d.Load(out.toUtf8().constData());
            QVERIFY(d.GetPages().GetCount() >= 1);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("saved summary is not a loadable PDF: %1").arg(e.what())));
        }
    }

    void sanitizationIsDisclosedAndVisibleInArtifact()
    {
        auto tmp = newTmp("w104d");
        QVERIFY(tmp);
        const QString out = tmp->filePath(QStringLiteral("summary.pdf"));
        QString err;
        QVERIFY(ReviewSummaryWriter::writePrintable(
            out, QStringLiteral("W2B Disclosure Summary"), hostileComments(),
            ReviewSummaryWriter::PrintOptions{}, &err));
        // Sanitizing with disclosure, never silent mangling: the artifact
        // carries the "shown as ?" note and the substituted glyphs.
        const QString text = extractedText(out);
        QVERIFY2(text.contains(QStringLiteral("shown as")),
                 "the sanitized export must disclose the substitution IN the "
                 "artifact (W1-04 honesty shape)");
        QVERIFY2(text.contains(QLatin1Char('?')),
                 "the substituted '?' must actually be drawn");
    }

    void winAnsiRepresentableContentSurvivesVerbatim()
    {
        auto tmp = newTmp("w104c");
        QVERIFY(tmp);
        const QString out = tmp->filePath(QStringLiteral("summary.pdf"));
        QString err;
        QVERIFY(ReviewSummaryWriter::writePrintable(
            out, QStringLiteral("W2B Benign Summary"), benignComments(),
            ReviewSummaryWriter::PrintOptions{}, &err));
        const QString text = extractedText(out);
        QVERIFY2(text.contains(QStringLiteral("Ren\u00e9")),
                 "Latin-1 supplement must pass through unmangled");
        QVERIFY2(text.contains(QStringLiteral("\u2014")),
                 "CP1252 specials (em-dash) must pass through unmangled");
        QVERIFY2(text.contains(QStringLiteral("\u20ac")),
                 "CP1252 specials (euro) must pass through unmangled");
    }

    // ── W1-05: the trust model is disclosed where overrides render ─────────
    void statusLineCarriesTheTrustModel()
    {
        auto tmp = newTmp("w105");
        QVERIFY(tmp);
        qputenv("GLYPHPDF_POLICY_PATH",
                writePolicy(*tmp, squatterPolicy(QStringLiteral("https://attacker.example/tsa")))
                    .toUtf8());
        auto& policy = gp::PolicyController::instance();
        policy.resetForTesting();
        policy.ensureLoaded();
        // W1-05 structural close: the squatter's file is NOT admin-owned, so
        // the gate refuses it — the honest disposition this probe now pins.
        QCOMPARE(policy.state(), gp::PolicyController::State::UntrustedOwner);
        QVERIFY2(!policy.isManaged(QStringLiteral("signing/tsaUrl")),
                 "the attacker URL must not reach enforcement");

        const QString note = gp::PolicyController::trustModelNote();
        QVERIFY2(note.contains(QStringLiteral("administrator-tier")),
                 "the canonical note must state the admin-ownership rule");
        QVERIFY2(note.contains(QStringLiteral("machine-trusted")),
                 "the canonical note must still name the residual "
                 "machine-trusted model where no ownership check applies");
        const QString line = policy.statusLine();
        QVERIFY2(line.contains(QStringLiteral("IGNORED")),
                 qPrintable(QStringLiteral(
                     "the untrusted-owner refusal must be disclosed: %1")
                     .arg(line)));
        QVERIFY2(line.contains(note.left(40)),
                 qPrintable(QStringLiteral(
                     "W1-05 REGRESSION: the status line where the policy "
                     "status renders no longer carries the trust-model "
                     "disclosure: %1").arg(line)));

        qputenv("GLYPHPDF_POLICY_PATH", QByteArray());
        policy.resetForTesting();
    }

    void supportBundleCarriesTrustModelField()
    {
        auto tmp = newTmp("w105b");
        QVERIFY(tmp);
        qputenv("GLYPHPDF_POLICY_PATH",
                writePolicy(*tmp, squatterPolicy(QStringLiteral("https://tsa.corp.example/rfc3161")))
                    .toUtf8());
        auto& policy = gp::PolicyController::instance();
        policy.resetForTesting();
        policy.ensureLoaded();

        const QString ini = tmp->filePath(QStringLiteral("user.ini"));
        QSettings user(ini, QSettings::IniFormat);
        const QJsonObject bundle = gp::SupportBundle::buildFromSettings(user);
        qputenv("GLYPHPDF_POLICY_PATH", QByteArray());
        policy.resetForTesting();

        const QJsonObject policySection = bundle.value(QStringLiteral("policy")).toObject();
        const QString trustModel = policySection.value(QStringLiteral("trustModel")).toString();
        QVERIFY2(trustModel.contains(QStringLiteral("administrator-tier")),
                 "W1-05 REGRESSION: the support bundle's policy section lost "
                 "the trustModel field (admin-ownership rule)");
        // The squatter file is refused: the bundle records the honest state.
        QCOMPARE(policySection.value(QStringLiteral("state")).toString(),
                 QStringLiteral("untrusted-owner-ignored"));
    }

    // ── F4: the TSA row reports the policy-EFFECTIVE state ─────────────────
    void tsaTouchpointReflectsPolicyEffectiveValue()
    {
        auto tmp = newTmp("f4");
        QVERIFY(tmp);
        qputenv("GLYPHPDF_POLICY_PATH",
                writePolicy(*tmp, squatterPolicy(QStringLiteral("https://tsa.corp.example/rfc3161")))
                    .toUtf8());
        // W1-05 structural close: this is an F4 pin (enumeration reflects the
        // POLICY-EFFECTIVE state), so the policy must be legitimately
        // enforcible — run under the disclosed assume-trusted seam. The
        // squatter path itself is pinned untrusted in statusLineCarries….
        qputenv("GLYPHPDF_POLICY_ASSUME_TRUSTED", "1");
        auto& policy = gp::PolicyController::instance();
        policy.resetForTesting();
        policy.ensureLoaded();

        const QString ini = tmp->filePath(QStringLiteral("user.ini"));
        QSettings user(ini, QSettings::IniFormat);
        QVERIFY(!user.contains(QStringLiteral("signing/tsaUrl")));  // user: none
        const auto tps = gp::NetworkTouchpoints::enumerate(user);
        qputenv("GLYPHPDF_POLICY_PATH", QByteArray());
        qunsetenv("GLYPHPDF_POLICY_ASSUME_TRUSTED");
        policy.resetForTesting();

        const gp::NetworkTouchpoint* tsa = nullptr;
        for (const auto& tp : tps)
            if (tp.id == QStringLiteral("tsa")) tsa = &tp;
        QVERIFY(tsa);
        QVERIFY2(tsa->enabled,
                 "F4 REGRESSION: the Network page / bundle shows the TSA "
                 "touchpoint DISABLED while the enforced policy URL makes the "
                 "next sign above B-B fetch it");
        QVERIFY2(tsa->disclosure.contains(QStringLiteral("policy")),
                 qPrintable(QStringLiteral(
                     "the disclosure should name the policy override: %1")
                     .arg(tsa->disclosure)));
    }

    void tsaTouchpointDisabledWithoutPolicyControl()
    {
        auto tmp = newTmp("f4c");
        QVERIFY(tmp);
        qputenv("GLYPHPDF_POLICY_PATH", QByteArray());
        auto& policy = gp::PolicyController::instance();
        policy.resetForTesting();
        const QString ini = tmp->filePath(QStringLiteral("user.ini"));
        QSettings user(ini, QSettings::IniFormat);
        const auto tps = gp::NetworkTouchpoints::enumerate(user);
        for (const auto& tp : tps)
            if (tp.id == QStringLiteral("tsa"))
                QVERIFY2(!tp.enabled,
                         "control: no user TSA and no policy must read Disabled");
    }

    // ── F6: the privacy note states the one path exception ─────────────────
    void privacyNoteDisclosesPolicyPathException()
    {
        auto tmp = newTmp("f6");
        QVERIFY(tmp);
        qputenv("GLYPHPDF_POLICY_PATH",
                writePolicy(*tmp, squatterPolicy(QString())).toUtf8());
        auto& policy = gp::PolicyController::instance();
        policy.resetForTesting();
        policy.ensureLoaded();
        const QString ini = tmp->filePath(QStringLiteral("user.ini"));
        QSettings user(ini, QSettings::IniFormat);
        const QJsonObject bundle = gp::SupportBundle::buildFromSettings(user);
        qputenv("GLYPHPDF_POLICY_PATH", QByteArray());
        policy.resetForTesting();

        const QString note = bundle.value(QStringLiteral("privacyNote")).toString();
        QVERIFY2(!note.contains(QStringLiteral("no file paths")),
                 qPrintable(QStringLiteral(
                     "F6 REGRESSION: the privacy note overclaims 'no file "
                     "paths' again while the policy disclosure embeds one: %1")
                     .arg(note)));
        QVERIFY2(note.contains(QStringLiteral("machine-policy location")),
                 qPrintable(QStringLiteral(
                     "the note must disclose the policy-path exception: %1").arg(note)));
    }
};

QTEST_MAIN(W2BProbeSummaryPolicy)
#include "W2BProbeSummaryPolicy.moc"
