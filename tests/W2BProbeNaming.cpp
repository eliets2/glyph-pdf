// SPDX-License-Identifier: MIT
// W2BProbeNaming.cpp — SWEEP-W2B INDEPENDENT verifier probe (W1-01 + FZ-4).
//
// Written by the W2B guarantee-verification-engine against the SAVED tip
// (feat/sweep-w2-verify-b @ 2d29a16). NOT the fix lane's repro: the hostile
// inputs here are chosen independently (different templates, different
// reserved names, length bound, benign controls). Asserts the FIXED contract:
//
//   W1-01: BatchPresetSchema::resolveNaming returns the RESOLVED result only
//     when it is a single bare path component (no '/', '\', ':', no '.'/'..'),
//     and BatchPresetCodec::parse refuses any preset whose output.naming
//     resolves otherwise (error names "output.naming"). One guard, inherited
//     by parse-time validation, the GUI pre-check and the worker capture.
//   FZ-4: the FINAL rendered result may not stem a reserved DOS device name
//     (CON/PRN/AUX/NUL/COM1-9/LPT1-9, any case, trailing dots/spaces ignored)
//     and may not exceed 240 chars — hostile values through ANY token are
//     covered because the check is on the render, template-agnostic.
//
// Negative controls (recorded in docs/audit/SWEEP-W2B-VERIFY-2026-09-20.md):
//   W1-01 NC base 9734e57^ — traversal/absolute templates parse again.
//   FZ-4  NC base e8e715f^  — reserved stems render again (W1-01 still fixed).

#include <QtTest/QtTest>
#include <QDate>
#include <QDir>

#include "core/BatchPreset.h"

using gp::BatchPreset;
namespace Codec = gp::BatchPresetCodec;
namespace Schema = gp::BatchPresetSchema;

namespace {

QByteArray presetJsonWithNaming(const QByteArray& naming)
{
    return QByteArray(
        "{\n"
        "  \"glyphpreset\": { \"schemaVersion\": 1, \"kind\": \"batch-preset\" },\n"
        "  \"id\": \"w2b-probe\",\n"
        "  \"name\": \"W2B Probe\",\n"
        "  \"created\": \"2026-09-20T00:00:00.000Z\",\n"
        "  \"modified\": \"2026-09-20T00:00:00.000Z\",\n"
        "  \"steps\": [ { \"op\": \"strip-metadata\", \"params\": {} } ],\n"
        "  \"output\": { \"naming\": \"") + naming + QByteArray("\", "
        "                \"onConflict\": \"ask\" }\n"
        "}\n");
}

} // namespace

class W2BProbeNaming : public QObject
{
    Q_OBJECT

private slots:
    // ── W1-01: hostile templates refused at parse (import boundary) ────────
    void parseRefusesHostileNamingTemplates()
    {
        const QList<QByteArray> hostile = {
        // NOTE: the backslash form (.. + backslash + evil_{n}.pdf) is NOT in
        // this list: the renderer drops the separator, so it resolves to the
        // bare in-directory name ..evil_{n}.pdf (contained) — pinned by the
        // launderedTemplatesStillResolveToBareNames invariant slot below.
            "....//....//evil_{n}.pdf",            // nested traversal
            "C:/Users/Public/evil_{n}.pdf",        // drive-absolute
            "C:\\Windows\\evil_{n}.pdf",           // drive-absolute backslash
            "\\\\server\\share\\evil_{n}.pdf",     // UNC
            "sub/dir/evil_{n}.pdf",                // directory component
            "evil.pdf:ads",                        // NTFS alternate data stream
        };
        for (const QByteArray& tmpl : hostile) {
            QString err;
            BatchPreset p;
            const bool ok = Codec::parse(presetJsonWithNaming(tmpl), &p, &err);
            QVERIFY2(!ok, qPrintable(QStringLiteral(
                "W1-01 REGRESSION: parse() ACCEPTED hostile naming '%1' "
                "(resolved '%2', err '%3') — the preset would escape the "
                "output directory at run time")
                .arg(QString::fromLatin1(tmpl), p.outputNaming, err)));
            QVERIFY2(err.contains(QStringLiteral("output.naming")),
                     qPrintable(QStringLiteral(
                         "refusal for '%1' must name the offending key, got: %2")
                         .arg(QString::fromLatin1(tmpl), err)));
        }
    }

    // Hostile templates whose hostility the renderer LAUNDERS away (e.g. a
    // backslash separator dropped during rendering) must still leave a RESULT
    // that is a bare in-directory file name — the containment INVARIANT is
    // what the W1-01 gate enforces, refusal is just its visible face.
    void launderedTemplatesStillResolveToBareNames()
    {
        // '..' + backslash + 'evil_{n}.pdf' renders with the separator
        // dropped: the resolved name must still be a single bare component
        // (no '/', '\', ':', not '.'/'..') — else the preset escapes the
        // output directory.
        QString err2;
        BatchPreset p;
        const bool ok = Codec::parse(presetJsonWithNaming("..\\evil_{n}.pdf"), &p, &err2);
        if (ok) {
            QVERIFY2(!p.outputNaming.contains(QLatin1Char('/'))
                         && !p.outputNaming.contains(QLatin1Char('\\'))
                         && !p.outputNaming.contains(QLatin1Char(':'))
                         && p.outputNaming != QLatin1String("..")
                         && p.outputNaming != QLatin1String("."),
                     qPrintable(QStringLiteral(
                         "W1-01 REGRESSION: laundered template resolved to a "
                         "non-bare name '%1'").arg(p.outputNaming)));
        }
    }

    // ── W1-01: the shared resolver is the choke point ──────────────────────
    void resolveNamingRejectsNonBareResults()
    {
        QString name, err;
        const QDate d(2026, 9, 20);
        const QList<QString> hostile = {
            QStringLiteral("../evil_{n}.pdf"),
            QStringLiteral("C:/evil_{n}.pdf"),
            QStringLiteral("sub/dir/evil_{n}.pdf"),
            QStringLiteral("a\\b.pdf"),
            QStringLiteral("x:y.pdf"),
        };
        for (const QString& tmpl : hostile) {
            err.clear();
            const bool ok = Schema::resolveNaming(tmpl, QStringLiteral("report"),
                                                  QStringLiteral("w2b"), 1, d,
                                                  &name, &err);
            QVERIFY2(!ok, qPrintable(QStringLiteral(
                "W1-01 REGRESSION: resolveNaming('%1') accepted; resolved '%2'")
                .arg(tmpl, name)));
            QVERIFY2(err.contains(QStringLiteral("output.naming")),
                     qPrintable(QStringLiteral("error must name output.naming, got: %1").arg(err)));
        }
        // The rendered RESULT is guarded too (independent of how the hostility
        // arrives): hostile values through the {basename} token must never
        // yield a result outside the bare-name contract.
        err.clear();
        const bool dotdot = Schema::resolveNaming(QStringLiteral("{basename}.pdf"),
                                                  QStringLiteral(".."), QStringLiteral("w2b"),
                                                  1, d, &name, &err);
        QVERIFY2(!dotdot || (!name.contains(QLatin1Char('/'))
                             && !name.contains(QLatin1Char('\\'))
                             && name != QLatin1String("..")),
                 qPrintable(QStringLiteral(
                     "W1-01 REGRESSION: basename '..' rendered non-bare '%1'").arg(name)));
    }

    // ── FZ-4: reserved DOS device names refused on the final render ────────
    void resolveNamingRejectsReservedDeviceStems()
    {
        QString name, err;
        const QDate d(2026, 9, 20);
        // Hostile values arriving through ANY token (template-agnostic gate):
        const QList<QString> reserved = {
            QStringLiteral("CON"), QStringLiteral("con"), QStringLiteral("Nul"),
            QStringLiteral("AUX"), QStringLiteral("PRN"), QStringLiteral("COM1"),
            QStringLiteral("com9"), QStringLiteral("LPT1"), QStringLiteral("lpt4"),
        };
        for (const QString& stem : reserved) {
            err.clear();
            const bool viaBasename = Schema::resolveNaming(
                QStringLiteral("{basename}.pdf"), stem, QStringLiteral("w2b"),
                1, d, &name, &err);
            QVERIFY2(!viaBasename, qPrintable(QStringLiteral(
                "FZ-4 REGRESSION: basename '%1' rendered '%2' — writing it "
                "targets the DEVICE while the batch reports success")
                .arg(stem, name)));

            err.clear();
            const bool viaLiteral = Schema::resolveNaming(
                stem + QStringLiteral(".pdf"), QStringLiteral("report"),
                QStringLiteral("w2b"), 1, d, &name, &err);
            QVERIFY2(!viaLiteral, qPrintable(QStringLiteral(
                "FZ-4 REGRESSION: literal template '%1.pdf' rendered '%2'")
                .arg(stem, name)));
        }
        // Win32 semantics: trailing dots/spaces are ignored before comparing.
        err.clear();
        QVERIFY2(!Schema::resolveNaming(QStringLiteral("{basename}.pdf"),
                                        QStringLiteral("NUL "), QStringLiteral("w2b"),
                                        1, d, &name, &err),
                 "reserved check must ignore trailing spaces (Win32)");
        err.clear();
        QVERIFY2(!Schema::resolveNaming(QStringLiteral("CON..pdf"),
                                        QStringLiteral("report"), QStringLiteral("w2b"),
                                        1, d, &name, &err),
                 "reserved check must ignore trailing dots (Win32)");
    }

    // ── FZ-4: bounded render length ─────────────────────────────────────────
    void resolveNamingRejectsOverlongRenders()
    {
        QString name, err;
        const QDate d(2026, 9, 20);
        const QString longBase(300, QChar('a'));
        err.clear();
        const bool ok = Schema::resolveNaming(QStringLiteral("{basename}.pdf"),
                                              longBase, QStringLiteral("w2b"),
                                              1, d, &name, &err);
        QVERIFY2(!ok, qPrintable(QStringLiteral(
            "FZ-4 REGRESSION: a %1-char render was accepted ('%2…') — it fails "
            "LATE in Win32 instead of refusing at the schema")
            .arg(name.size()).arg(name.left(20))));
        QVERIFY2(err.contains(QStringLiteral("240")),
                 qPrintable(QStringLiteral("length error should name the bound, got: %1").arg(err)));
    }

    // ── controls: the honest path must keep working (no over-blocking) ─────
    void benignNamingStillResolves()
    {
        QString name, err;
        const QDate d(2026, 9, 20);
        QVERIFY2(Schema::resolveNaming(QStringLiteral("{basename}_{preset}_{n}.pdf"),
                                       QStringLiteral("Quarterly Report"), QStringLiteral("opt"),
                                       3, d, &name, &err),
                 qPrintable(err));
        QCOMPARE(name, QStringLiteral("Quarterly Report_opt_3.pdf"));

        QVERIFY(Schema::resolveNaming(QStringLiteral("{basename}.pdf"),
                                      QStringLiteral("résumé"), QStringLiteral("w2b"),
                                      1, d, &name, &err));
        QVERIFY(name.startsWith(QStringLiteral("résumé")));

        // 240-char renders stay legal (the bound is the refusal threshold).
        const QString okBase(230, QChar('b'));
        QVERIFY(Schema::resolveNaming(QStringLiteral("{basename}.pdf"), okBase,
                                      QStringLiteral("w2b"), 1, d, &name, &err));

        // Non-reserved names that merely CONTAIN reserved letters pass.
        QVERIFY(Schema::resolveNaming(QStringLiteral("{basename}.pdf"),
                                      QStringLiteral("console"), QStringLiteral("w2b"),
                                      1, d, &name, &err));
        QCOMPARE(name, QStringLiteral("console.pdf"));
    }

    void parseStillAcceptsBenignPreset()
    {
        QString err;
        BatchPreset p;
        QVERIFY2(Codec::parse(presetJsonWithNaming("{basename}_{n}.pdf"), &p, &err),
                 qPrintable(err));
        QCOMPARE(p.outputNaming, QStringLiteral("{basename}_{n}.pdf"));
        QCOMPARE(p.onConflict, QStringLiteral("ask"));
    }
};

QTEST_MAIN(W2BProbeNaming)
#include "W2BProbeNaming.moc"
