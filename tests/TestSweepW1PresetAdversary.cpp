// SPDX-License-Identifier: MIT
// TestSweepW1PresetAdversary.cpp
//
// SWEEP-W1 ADVERSARY (2026-09-19) — batch-presets pipeline, hostile preset
// JSON. REVIEW-ONLY repro: these tests are written to FAIL against the
// candidate (feat/parity-glm @ a3a9317) to prove the defect; a fix flips them
// green without any edit here.
//
// FINDING W1-P1 (CONFIRMED, codec-level): `output.naming` template literals
// are never separator/containment-checked. The plan §3.6 rule ("no path
// separator can be smuggled through a token") is enforced ONLY on token
// VALUES (sanitizeNameComponent); the template text itself may carry `/`,
// `\\`, `:`, `..` verbatim, and the resolved name is joined with
// QDir(outDir).filePath(...) at BOTH resolution sites (BatchMode::onRunClicked
// overwrite pre-check and the captured worker output), which neither
// normalizes `..` nor rejects absolute results. Chain:
//   parse() accepts naming "../evil.pdf"  (line ~640: resolveNaming check is
//     syntax-only — tokens + ".pdf" suffix)
//   → resolveOutputPath(): QDir(outDir).filePath("../evil.pdf")
//   → onConflict "overwrite" preset skips EVERY interactive overwrite confirm
//     (presetOverwriteConfirmed)
//   → worker: SafeSave::commitFileToDestination(current, outputPath)
//   → arbitrary file write/overwrite outside the output directory with
//     attacker-influenced PDF bytes (watermark text op is schema-validated
//     but attacker-chosen).
// Exploit narrative: a hostile .glyphpreset.json distributed as
// "web-optimize.glyphpreset.json" (id must match the stem — attacker controls
// both) is imported and run by a victim; the batch writes/overwrites files at
// attacker-chosen relative (or via drive letters, absolute) paths silently.
//
// The existing pin (TestBatchPresets::namingTokensResolveAndSanitize) covers
// separator smuggling through TOKEN VALUES only — the template literal is
// unpinned. These tests close that gap; they are the failing repro.

#include <QtTest/QtTest>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "core/BatchPreset.h"

using gp::BatchPreset;
namespace BatchPresetCodec = gp::BatchPresetCodec;
namespace BatchPresetSchema = gp::BatchPresetSchema;

namespace {

// A minimal schema-v1 preset body with the given naming template. Every other
// field is schema-clean so the ONLY thing under test is the naming template.
QByteArray presetJsonWithNaming(const QByteArray& naming)
{
    // The naming value is embedded verbatim (no JSON escaping needed for the
    // characters under test: '/', '\\', ':', '.', '_').
    return QByteArray(
        "{\n"
        "  \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "  \"id\": \"traversal-probe\",\n"
        "  \"name\": \"Traversal Probe\",\n"
        "  \"created\": \"2026-09-19T00:00:00.000Z\",\n"
        "  \"modified\": \"2026-09-19T00:00:00.000Z\",\n"
        "  \"steps\": [ { \"op\": \"strip-metadata\", \"params\": {} } ],\n"
        "  \"output\": { \"naming\": \"") + naming + QByteArray("\", "
        "                \"onConflict\": \"overwrite\" }\n"
        "}\n");
}

} // namespace

class TestSweepW1PresetAdversary : public QObject
{
    Q_OBJECT

private slots:
    // ── FAILING REPRO (codec): the parser must refuse traversal templates ──
    //
    // Expected (post-fix): parse() fails with an output.naming error naming
    // the containment rule. Today: parse() SUCCEEDS — the hostile preset
    // loads, and the run chain below the codec commits to the escaped path.
    void parseRefusesParentTraversalNamingTemplate()
    {
        QString err;
        BatchPreset p;
        const bool ok = BatchPresetCodec::parse(
            presetJsonWithNaming("../evil_{n}.pdf"), &p, &err);
        QVERIFY2(!ok,
                 qPrintable(QStringLiteral(
                     "SECURITY DEFECT: parse() ACCEPTED a naming template with "
                     "'..', i.e. output escapes the chosen output directory at "
                     "run time (resolved name: %1; err: %2)")
                     .arg(p.outputNaming, err)));
        QVERIFY(err.contains(QStringLiteral("output.naming")));
    }

    void parseRefusesAbsoluteNamingTemplate()
    {
        QString err;
        BatchPreset p;
        // A drive-letter absolute template — QDir::filePath returns an
        // absolute operand as-is, so this writes anywhere on the machine.
        const bool ok = BatchPresetCodec::parse(
            presetJsonWithNaming("C:/Users/Public/evil_{n}.pdf"), &p, &err);
        QVERIFY2(!ok,
                 qPrintable(QStringLiteral(
                     "SECURITY DEFECT: parse() ACCEPTED an absolute naming "
                     "template (resolved name: %1; err: %2)")
                     .arg(p.outputNaming, err)));
    }

    // ── FAILING REPRO (pure resolution): the shared resolver is the defect ──
    //
    // Root-cause statement: the sanitize rule lives at token-value level
    // (sanitizeNameComponent) instead of at the RESOLVED RESULT level. Patch
    // once here (reject separator/'..'/drive-absolute RESULTS), and every
    // caller — GUI pre-check, worker capture, parse-time validation — is
    // fixed by the same guard (ponytail ladder: patch the shared function,
    // never the callers).
    void resolveNamingRefusesTraversalAndAbsoluteResults()
    {
        QString name;
        QString err;

        const bool rel = BatchPresetSchema::resolveNaming(
            QStringLiteral("../evil_{n}.pdf"), QStringLiteral("report"),
            QStringLiteral("probe"), 1, QDate(2026, 9, 19), &name, &err);
        QVERIFY2(!rel, qPrintable(QStringLiteral(
            "SECURITY DEFECT: resolveNaming() accepted the '../' template; "
            "resolved name = %1").arg(name)));

        err.clear();
        const bool abs = BatchPresetSchema::resolveNaming(
            QStringLiteral("C:/evil_{n}.pdf"), QStringLiteral("report"),
            QStringLiteral("probe"), 1, QDate(2026, 9, 19), &name, &err);
        QVERIFY2(!abs, qPrintable(QStringLiteral(
            "SECURITY DEFECT: resolveNaming() accepted the drive-absolute "
            "template; resolved name = %1").arg(name)));

        err.clear();
        const bool sep = BatchPresetSchema::resolveNaming(
            QStringLiteral("sub/dir/evil_{n}.pdf"), QStringLiteral("report"),
            QStringLiteral("probe"), 1, QDate(2026, 9, 19), &name, &err);
        QVERIFY2(!sep, qPrintable(QStringLiteral(
            "SECURITY DEFECT: resolveNaming() accepted a template containing "
            "a path separator outside any token; resolved name = %1").arg(name)));
    }

    // ── EXPLOITABILITY PIN (passes today; documents the join semantics) ────
    //
    // Proves the run chain has NO downstream containment: QDir::filePath
    // neither normalizes '..' nor rejects absolute operands, so the resolved
    // hostile name reaches SafeSave::commitFileToDestination verbatim. When
    // the resolver is fixed, this pin still passes (it pins Qt semantics,
    // not the defect).
    void joinSemanticsPreserveTraversalProbe()
    {
        const QString outDir = QDir::temp().absolutePath();

        const QString rel = QDir(outDir).filePath(QStringLiteral("../escaped.pdf"));
        const QFileInfo relInfo(rel);
        const QDir relCanon(relInfo.absoluteDir().canonicalPath());
        const QDir outCanon(QDir(outDir).canonicalPath());
        QVERIFY2(relCanon != outCanon,
                 qPrintable(QStringLiteral(
                     "expected the '..' operand to resolve OUTSIDE the output "
                     "directory (joined: %1)").arg(rel)));

        const QString abs = QDir(outDir).filePath(QStringLiteral("C:/somewhere/x.pdf"));
        QCOMPARE(abs, QStringLiteral("C:/somewhere/x.pdf"));   // returned as-is
    }
};

QTEST_MAIN(TestSweepW1PresetAdversary)
#include "TestSweepW1PresetAdversary.moc"
