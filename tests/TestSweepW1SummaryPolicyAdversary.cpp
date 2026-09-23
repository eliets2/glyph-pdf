// SPDX-License-Identifier: MIT
// TestSweepW1SummaryPolicyAdversary.cpp
//
// SWEEP-W1 ADVERSARY (2026-09-19) — printable-summary robustness probes and
// the machine-policy squat probe. These are PROBES: they pin the defended
// behavior and would expose a defect as a failure. Written against
// feat/parity-glm @ a3a9317; they are expected GREEN unless a probe finds a
// real defect (in which case the failure IS the finding evidence).
//
// PROBE GROUP P1 — printable summaries under adversarial annotation content
// (attack-surface brief: "annotation content injection into the rendered
// summary PDF — layout injection, unbounded loops on adversarial annot
// dicts"). The writer draws via PoDoFo DrawText (escaped PDF strings — no
// operator injection is possible), wraps at a fixed 92 columns with a
// guaranteed-progress loop, and paginates with ensureRoom. The probes drive
// the writer with worst-case strings and verify: termination, sanity of the
// artifact (opens, bounded page count), and content survival (no crash).
//
// PROBE GROUP P2 — machine-policy squat (attack-surface brief: "policy file
// as attack vector"). Demonstrates that an attacker-writable policy.json (a
// standard user CAN create %PROGRAMDATA%\GlyphPDF\policy.json on a default
// Windows install — the directory is auto-creatable and the creator owns the
// file) is loaded and ENFORCED (signing/tsaUrl + signing/padesLevel are the
// two enforced keys). The finding is the DESIGN/ACL posture: no ownership or
// integrity check exists at load; the payload is a machine-wide signing
// downgrade (level B_B) or TSA redirect. The probe uses the documented
// GLYPHPDF_POLICY_PATH test seam to prove the enforce path from an
// attacker-planted file.

#include <QtTest/QtTest>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <memory>

#include <podofo/podofo.h>

#include "core/AnnotationTypes.h"
#include "core/PolicyController.h"
#include "engines/ReviewSummaryWriter.h"

namespace {

using gp::PolicyController;
// AnnotationItem is deliberately namespace-free (AnnotationTypes idiom).

AnnotationItem makeAdversarialComment(int page, const QString &author,
                                      const QString &text)
{
    AnnotationItem c;
    c.pageIndex = page;
    c.author = author;
    c.text = text;
    c.creationDate = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    return c;
}

QList<AnnotationItem> adversarialCommentSet()
{
    QList<AnnotationItem> comments;
    // Rich-text-looking payloads (must be drawn as TEXT, never interpreted).
    comments << makeAdversarialComment(0, QStringLiteral("<b>Eve</b>"),
                                       QStringLiteral("<script>alert(1)</script>"));
    // Operator-ish payloads.
    comments << makeAdversarialComment(
        0, QStringLiteral("T* ET BT /F1 48 Tf"),
        QStringLiteral(") Tj (injected) Tj ("));
    // Control characters: 0x00 only — NUL passes the writer (C-string
    // truncation); 0x01-0x1F (including TAB) belong to the ENCODING finding
    // (P1d/triage), not to the loop/injection pin this set drives.
    QString ctrl;
    for (int i = 0; i < 64; ++i)
        ctrl += QString(QChar(0)) + QStringLiteral("X")
                + QString(QChar(i ? i + 0x20 : 0x21)) + QStringLiteral("Y");
    comments << makeAdversarialComment(1, ctrl, ctrl + ctrl);
    // The wrap-progress hazard: one paragraph, no spaces, huge.
    QString noSpaces;
    noSpaces.fill(QLatin1Char('A'), 500000);
    comments << makeAdversarialComment(1, QStringLiteral("bulk"), noSpaces);
    // Newline/spaced wrap hazard with pathological spacing.
    QString spaced;
    for (int i = 0; i < 100000; ++i)
        spaced += QStringLiteral("a  ");
    comments << makeAdversarialComment(2, QStringLiteral("spaces"), spaced);
    // Empty author / empty text (the honest "(no author)" branch).
    comments << makeAdversarialComment(2, QString(), QString());
    return comments;
}

QList<AnnotationItem> nonWinAnsiCommentSet()
{
    // Ordinary, non-malicious content: one CJK comment. Any review of a
    // document annotated in a non-Latin script hits exactly this input.
    QList<AnnotationItem> comments;
    comments << makeAdversarialComment(0, QStringLiteral("中文作者"),
                                       QStringLiteral("签名内容 — 请查收"));
    comments << makeAdversarialComment(1, QStringLiteral("Alice"),
                                       QStringLiteral("Plain Latin comment."));
    return comments;
}

} // namespace

class TestSweepW1SummaryPolicyAdversary : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_tmpDir;

private slots:
    void initTestCase() {
        for (int attempt = 0; attempt < 3 && !m_tmpDir; ++attempt)
            m_tmpDir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tmpDir && m_tmpDir->isValid());
    }

    // P1a: the writer terminates and produces a sane artifact under the
    // adversarial set (bounded by input size, no hang, no crash, no
    // operator injection).
    void writePrintableTerminatesOnAdversarialComments()
    {
        const QString out = m_tmpDir->filePath(QStringLiteral("w1_summary_adv.pdf"));
        QFile::remove(out);

        QElapsedTimer clock;
        clock.start();
        QString err;
        const bool ok = ReviewSummaryWriter::writePrintable(
            out, QStringLiteral("Adversarial <doc>.pdf & \"title\""),
            adversarialCommentSet(), ReviewSummaryWriter::PrintOptions(), &err);
        const qint64 elapsedMs = clock.elapsed();

        QVERIFY2(ok, qPrintable(QStringLiteral("writer failed: %1").arg(err)));
        // Generous ceiling — the probe cares about "no pathological runtime",
        // not absolute speed (~800k body chars must wrap+draw in seconds).
        QVERIFY2(elapsedMs < 120000,
                 qPrintable(QStringLiteral(
                     "SECURITY DEFECT (unbounded loop): writePrintable took "
                     "%1 ms on the adversarial set — the wrap/pagination "
                     "loops are not progress-guaranteed for hostile input.")
                     .arg(elapsedMs)));
        const QFileInfo fi(out);
        QVERIFY(fi.exists() && fi.size() > 0);
    }

    // ── FAILING REPRO (P1d): non-WinAnsi annotation content kills the whole
    // export ────────────────────────────────────────────────────────────────
    //
    // CONFIRMED against a3a9317 (see encodingTriggerTriage for the exact
    // trigger class): PoDoFo 1.1.0's standard-14 Helvetica cannot encode the
    // payload, DrawText throws PdfErrorCode::InvalidFontData ("The provided
    // string can't be converted to CID encoding", PdfEncoding.cpp:106), and
    // writePrintable fails — the ENTIRE summary export dies because ONE
    // comment carries a TAB (0x09), CJK or emoji character. Attacker
    // narrative: send a victim a PDF with a single such annotation; their
    // printable review summary (and print surface) is dead — deterministic
    // availability failure, no crash, fail-closed. Expected post-fix: the
    // export succeeds (CID-capable font or per-string sanitize/replace), or
    // at minimum degrades the STRING, never the FILE.
    void writePrintableSurvivesNonWinAnsiComments()
    {
        const QString out = m_tmpDir->filePath(QStringLiteral("w1_summary_cjk.pdf"));
        QFile::remove(out);
        QString err;
        const bool ok = ReviewSummaryWriter::writePrintable(
            out, QStringLiteral("contract_review.pdf"),
            nonWinAnsiCommentSet(), ReviewSummaryWriter::PrintOptions(), &err);
        QVERIFY2(ok, qPrintable(QStringLiteral(
            "SECURITY DEFECT (availability): the printable summary export "
            "FAILS ENTIRELY on ordinary non-Latin annotation content "
            "(error: %1). One CJK comment — benign, spec-legal /T and "
            "/Contents — denies the whole review-summary and print "
            "feature.").arg(err)));
    }

    // The same denial with the most mundane trigger imaginable: a TAB
    // character inside the comment text.
    void writePrintableSurvivesTabInCommentText()
    {
        const QString out = m_tmpDir->filePath(QStringLiteral("w1_summary_tab.pdf"));
        QFile::remove(out);
        QString err;
        const bool ok = ReviewSummaryWriter::writePrintable(
            out, QStringLiteral("t.pdf"),
            QList<AnnotationItem>()
                << makeAdversarialComment(0, QStringLiteral("Alice"),
                                          QStringLiteral("col1\tcol2")),
            ReviewSummaryWriter::PrintOptions(), &err);
        QVERIFY2(ok, qPrintable(QStringLiteral(
            "SECURITY DEFECT (availability): a single TAB in a comment text "
            "denies the whole summary export (error: %1).").arg(err)));
    }

    // P1b: renderEntries — the testable layout seam — carries the content
    // 1:1 (wrap may only drop skipped whitespace; it must never AMPLIFY).
    void renderEntriesBoundedOnAdversarialContent()
    {
        const auto set = adversarialCommentSet();
        qint64 inputChars = 0;
        for (const auto &c : set)
            inputChars += qint64(c.author.size()) + c.text.size();

        const auto entries = ReviewSummaryWriter::renderEntries(set);
        QCOMPARE(entries.size(), set.size());
        qint64 totalChars = 0;
        for (const auto &e : entries)
            totalChars += e.heading.size() + e.body.size();
        // Headings add status/date furniture (~64 chars each); the wrap may
        // drop spaces but never amplifies.
        QVERIFY2(totalChars <= inputChars + set.size() * 64,
                 qPrintable(QStringLiteral(
                     "SECURITY DEFECT (layout amplification): rendered output "
                     "%1 chars exceeds the adversarial input bound %2.")
                     .arg(totalChars).arg(inputChars + set.size() * 64)));
    }

    // P1c: the artifact re-opens as a valid PDF (independent read path) and
    // its page count is bounded by the content, not attacker-amplified.
    void summaryArtifactReopensAndIsBounded()
    {
        const QString out = m_tmpDir->filePath(QStringLiteral("w1_summary_adv.pdf"));
        if (!QFileInfo::exists(out))
            QSKIP("P1a artifact absent (write failed/skipped earlier)");
        QFile raw(out);
        QVERIFY(raw.open(QIODevice::ReadOnly));
        const QByteArray bytes = raw.readAll();
        raw.close();
        // Raw-bytes read path: header + EOF marker.
        QVERIFY(bytes.startsWith("%PDF-"));
        QVERIFY(bytes.contains("%%EOF"));
        // PoDoFo independent read path: the artifact re-opens and the page
        // count is bounded by the content (~5056 wrap lines for the 500k-char
        // body alone at 92 chars/line → low hundreds of sheets). An
        // amplification defect shows up as a runaway count; an unbounded-loop
        // defect never reaches here at all.
        const int pages = PoDoFoPageCount(bytes);
        QVERIFY2(pages >= 1,
                 qPrintable(QStringLiteral(
                     "the adversarial-summary artifact does not re-open "
                     "(pages=%1)").arg(pages)));
        QVERIFY2(pages <= 500,
                 qPrintable(QStringLiteral(
                     "SECURITY DEFECT (pagination amplification): the "
                     "adversarial set paginated to %1 sheets.").arg(pages)));
    }

    // P2: an attacker-planted policy file is loaded and ENFORCED. The probe
    // documents the squat: on a default Windows install a standard user can
    // create %PROGRAMDATA%\GlyphPDF\policy.json (auto-creatable dir, creator
    // owns the file) and GlyphPDF applies it machine-wide with no ownership
    // or integrity check at load. Payload: downgrade signing/padesLevel to
    // B_B and/or redirect signing/tsaUrl — the two keys isEnforcedKey() wires
    // into EVERY sign/certify dispatch (SecurityController::readSigningConfig).
    void attackerPlantedPolicyFileIsLoadedAndEnforced()
    {
        const QString path = m_tmpDir->filePath(QStringLiteral("policy.json"));
        QJsonObject root;
        root.insert(QStringLiteral("schemaVersion"), 1);
        QJsonObject settings;
        settings.insert(QStringLiteral("signing/tsaUrl"),
                        QStringLiteral("https://attacker.example/tsa"));
        settings.insert(QStringLiteral("signing/padesLevel"),
                        QStringLiteral("B-B"));   // downgrade payload
        settings.insert(QStringLiteral("signing/tsaUrlIsNotAKey"),
                        QStringLiteral("ignored-and-disclosed"));
        root.insert(QStringLiteral("settings"), settings);
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(QJsonDocument(root).toJson());
        f.close();

        PolicyController::instance().resetForTesting();
        PolicyController::instance().load(path);

        QCOMPARE(PolicyController::instance().state(),
                 PolicyController::State::Loaded);
        QVERIFY(PolicyController::instance().isManaged(QStringLiteral("signing/tsaUrl")));
        QVERIFY(PolicyController::instance().isManaged(QStringLiteral("signing/padesLevel")));
        // The two enforced keys carry the attacker values into every
        // sign/certify dispatch via effectiveValue (the squat payload works).
        QCOMPARE(PolicyController::instance()
                     .effectiveValue(QStringLiteral("signing/tsaUrl"),
                                     QStringLiteral("https://corp.tsa"))
                     .toString(),
                 QStringLiteral("https://attacker.example/tsa"));
        QCOMPARE(PolicyController::instance()
                     .effectiveValue(QStringLiteral("signing/padesLevel"),
                                     QStringLiteral("B-T"))
                     .toString(),
                 QStringLiteral("B-B"));
        // The disclosure floor that DOES exist: the unknown key is disclosed
        // and the status line names the attacker-controlled path.
        QCOMPARE(PolicyController::instance().unrecognizedKeys().size(), 1);
        QVERIFY(PolicyController::instance().statusLine().contains(path));
        PolicyController::instance().resetForTesting();
    }

    // Diagnostic triage: which single-content member kills the writer?
    // Always green — this slot only logs per-member outcomes for the audit.
    void encodingTriggerTriage()
    {
        struct Member { const char* label; QString author; QString text; };
        const QList<Member> members = {
            { "plain-latin",      QStringLiteral("Alice"), QStringLiteral("Hello.") },
            { "html-lookalike",   QStringLiteral("<b>Eve</b>"), QStringLiteral("<script>alert(1)</script>") },
            { "operator-ish",     QStringLiteral("T* ET BT /F1 48 Tf"), QStringLiteral(") Tj (injected) Tj (") },
            { "ctrl-0x00-only",   QStringLiteral("A"), QStringLiteral("X") + QString(QChar(0)) + QStringLiteral("Y") },
            { "ctrl-0x01-only",   QStringLiteral("A"), QStringLiteral("X") + QString(QChar(1)) + QStringLiteral("Y") },
            { "ctrl-0x1f-only",   QStringLiteral("A"), QStringLiteral("X") + QString(QChar(0x1f)) + QStringLiteral("Y") },
            { "ctrl-0x09-tab",    QStringLiteral("A"), QStringLiteral("X") + QString(QChar(0x09)) + QStringLiteral("Y") },
            { "latin1-0xe9",      QStringLiteral("Ren\xe9\xe9"), QStringLiteral("caf\xe9 cr\xe8me") },
            { "cjk-text",         QStringLiteral("Alice"), QStringLiteral("签名内容") },
            { "cjk-author",       QStringLiteral("中文作者"), QStringLiteral("Hello.") },
            { "emoji-u1f600",     QStringLiteral("A"), QStringLiteral("x") + QString(QChar(0xD83D)) + QString(QChar(0xDE00)) },
            { "empty-author",     QString(), QStringLiteral("Hello.") },
            { "empty-text",       QStringLiteral("Alice"), QString() },
        };
        for (const Member &m : members) {
            const QString out = m_tmpDir->filePath(
                QStringLiteral("w1_triage_%1.pdf").arg(QString::fromLatin1(m.label)));
            QFile::remove(out);
            QString err;
            const bool ok = ReviewSummaryWriter::writePrintable(
                out, QStringLiteral("t.pdf"),
                QList<AnnotationItem>() << makeAdversarialComment(0, m.author, m.text),
                ReviewSummaryWriter::PrintOptions(), &err);
            qInfo() << "TRIAGE" << m.label
                    << (ok ? "OK" : QStringLiteral("FAIL: %1").arg(err));
        }
        QSKIP("diagnostic slot — outcomes logged above");
    }

private:
    // Minimal independent reader: parse the /Count of the page tree with
    // PoDoFo (the writer's own backend — a mismatch here is a real defect).
    static int PoDoFoPageCount(const QByteArray &bytes)
    {
        QTemporaryFile tmp;
        if (!tmp.open()) return -1;
        tmp.write(bytes);
        tmp.flush();
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(tmp.fileName().toUtf8().constData());
            return static_cast<int>(doc.GetPages().GetCount());
        } catch (const std::exception &e) {
            qWarning() << "PoDoFo reopen failed:" << e.what();
            return -2;
        }
    }
};

QTEST_MAIN(TestSweepW1SummaryPolicyAdversary)
#include "TestSweepW1SummaryPolicyAdversary.moc"
