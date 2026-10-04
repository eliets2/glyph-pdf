// SPDX-License-Identifier: Apache-2.0
// §9.13 honesty regression test — re-scoped by font-subsetting-plan-2026-10-01 §5.1.
//
// BOTH compress passes are now implemented in the backend:
//   * "Remove unused objects" — trailer-rooted reachability sweep (21a387c);
//   * "Subset fonts" — keep-CID blank-glyph TrueType subsetter (route A,
//     feat/font-subset-tt): unused glyphs blanked, glyph numbering preserved.
// MRC remains the only Degraded compress capability (needs OCR-pipeline page
// images).
//
// The pinned contracts therefore become:
//   1. the unsupported-pass seam explains MRC-only unavailability (the retired
//      R12 "font subsetting not implemented" wording is gone),
//   2. subset fonts: ENABLED, default UNCHECKED (user opt-in — it mutates font
//      programs), carrying the SCOPE disclosure (TrueType covered; CFF/Type1/
//      OpenType, unprovable usage and signed documents left untouched),
//   3. remove unused: enabled, checked, options follow the checkbox,
//   4. no preset checks or disables subset fonts (and never overrides the
//      user's choice),
//   5. the options reaching the engine honor the checkbox state exactly,
//   6. the size row stays explicitly labeled as an estimate.
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QDebug>
#include <QLabel>
#include <QToolButton>

#include "modes/CompressDialog.h"
#include "core/AppContext.h"
#include "mocks/MockPdfEditorEngine.h"

namespace {

// Records the OptimizeOptions the dialog hands to the engine. Subclassing the
// shared mock (rather than modifying it) keeps this test self-contained.
class RecordingEditorEngine : public MockPdfEditorEngine {
public:
    OptimizeOptions lastEstimateOpts;
    bool sawEstimate = false;

    OptimizeEstimate estimateOptimization(const OptimizeOptions &o) override {
        lastEstimateOpts = o;
        sawEstimate = true;
        return OptimizeEstimate{};
    }
};

// Locate the checkboxes by their (untranslated in tests) user-facing text.
QCheckBox* findBox(const gp::CompressDialog &dlg, const QString &text)
{
    const auto boxes = dlg.findChildren<QCheckBox*>();
    for (auto *b : boxes)
        if (b->text() == text)
            return b;
    return nullptr;
}

QStringList stateViolations(const gp::CompressDialog &dlg)
{
    QStringList v;
    auto *subset = findBox(dlg, QStringLiteral("Subset fonts"));
    auto *remove = findBox(dlg, QStringLiteral("Remove unused objects"));
    if (!subset)
        v << QStringLiteral("the 'Subset fonts' checkbox is missing (the route-A "
                            "subsetter ships it as a real, opt-in pass)");
    if (!remove)
        v << QStringLiteral("the 'Remove unused objects' checkbox is missing");
    if (subset) {
        // The pass is implemented — a disabled checkbox would be the R12
        // placeholder lying in the other direction now.
        if (!subset->isEnabled())
            v << QStringLiteral("'Subset fonts' is DISABLED but the pass is "
                                "implemented in this build");
        // It mutates font programs: user opt-in only, never default-checked.
        if (subset->isChecked())
            v << QStringLiteral("'Subset fonts' defaults CHECKED — the pass must "
                                "only run on explicit user choice");
        const QString scope = gp::CompressDialog::subsetScopeExplanation();
        if (subset->toolTip() != scope)
            v << QStringLiteral("'Subset fonts' tooltip does not carry the canonical "
                                "scope disclosure");
        if (subset->statusTip() != scope)
            v << QStringLiteral("'Subset fonts' status tip does not carry the "
                                "canonical scope disclosure");
    }
    // "Remove unused objects" is implemented — its sweep contract is pinned in
    // TestCompressJpegReencode; its enabled state is asserted explicitly below.
    return v;
}

} // namespace

class TestCompressDialogHonesty : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestCompressDialogHonesty"));
    }

    // The seam now explains the only compress capability that still cannot run
    // in this dialog: MRC (Degraded — needs OCR-pipeline page images). The
    // retired R12 wording ("font subsetting ... not implemented") must be gone:
    // both passes it named are implemented.
    void unsupportedPassExplanationIsHonest() {
        const QString text = gp::CompressDialog::unsupportedPassExplanation();
        QVERIFY2(!text.trimmed().isEmpty(),
                 "unsupportedPassExplanation() must not be empty");
        QVERIFY2(text.contains(QStringLiteral("MRC"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "the seam must explain the MRC-only unavailability — the "
                     "compress passes are implemented; got: %1").arg(text)));
        QVERIFY2(text.contains(QStringLiteral("not available"), Qt::CaseInsensitive)
                 || text.contains(QStringLiteral("requires"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "the seam must state why MRC cannot run here; got: %1").arg(text)));
        QVERIFY2(!text.contains(QStringLiteral("font subsetting"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "the retired R12 wording must not survive — font subsetting "
                     "is implemented; got: %1").arg(text)));
    }

    // Route A shipped: the checkbox is enabled for real, defaults unchecked
    // (the pass mutates font programs — explicit opt-in), and carries the
    // scope disclosure instead of an availability excuse.
    void subsetFontsFollowsUserChoiceWithScopeDisclosure() {
        gp::CompressDialog dlg(nullptr);
        const QStringList v = stateViolations(dlg);
        QVERIFY2(v.isEmpty(), qPrintable(v.join(QStringLiteral("; "))));
        auto *subset = findBox(dlg, QStringLiteral("Subset fonts"));
        QVERIFY2(subset, "the 'Subset fonts' checkbox is missing");
        const QString scope = gp::CompressDialog::subsetScopeExplanation();
        QVERIFY2(subset->toolTip().contains(QStringLiteral("TrueType")),
                 "the scope disclosure must name the covered programs (TrueType)");
        QVERIFY2(subset->toolTip().contains(QStringLiteral("left untouched")),
                 "the scope disclosure must name which fonts are left untouched");
        QVERIFY2(!subset->toolTip().contains(QStringLiteral("not available"),
                                             Qt::CaseInsensitive),
                 "an implemented pass must not be explained with an "
                 "availability excuse");
        // The remove-unused pass stays enabled+checked (21a387c contract).
        auto *remove = findBox(dlg, QStringLiteral("Remove unused objects"));
        QVERIFY2(remove, "the 'Remove unused objects' checkbox is missing");
        QVERIFY2(remove->isEnabled(),
                 "'Remove unused objects' must be ENABLED — the sweep is implemented");
        QVERIFY2(remove->isChecked(),
                 "'Remove unused objects' must default CHECKED — the sweep "
                 "honors the user's choice");
    }

    // Switching presets (Screen/Ebook/Printer/Custom) must never silently
    // check "Subset fonts" (font-program mutation is opt-in) — and since the
    // pass is implemented, never disable or uncheck it either. The user's
    // choice survives preset clicks exactly like the remove-unused choice.
    void presetsNeverTouchTheSubsetChoice() {
        gp::CompressDialog dlg(nullptr);
        auto *subset = findBox(dlg, QStringLiteral("Subset fonts"));
        QVERIFY2(subset, "the 'Subset fonts' checkbox is missing");

        int presetCards = 0;
        const auto buttons = dlg.findChildren<QToolButton*>();
        for (auto *b : buttons) {
            if (!b->isCheckable())
                continue;
            ++presetCards;
            // User opted in before touching presets.
            subset->setChecked(true);
            b->click();
            QStringList v;
            if (!subset->isEnabled())
                v << QStringLiteral("'Subset fonts' was DISABLED by a preset");
            if (!subset->isChecked())
                v << QStringLiteral("a preset silently unchecked the user's "
                                    "opt-in 'Subset fonts' choice");
            QVERIFY2(v.isEmpty(),
                     qPrintable(QStringLiteral("after clicking preset '%1': %2")
                                    .arg(b->text().section(QLatin1Char('\n'), 0, 0),
                                         v.join(QStringLiteral("; ")))));
            // And with the user opted out, no preset may check it for them.
            subset->setChecked(false);
            b->click();
            QVERIFY2(!subset->isChecked(),
                     qPrintable(QStringLiteral(
                         "preset '%1' silently checked 'Subset fonts'")
                             .arg(b->text().section(QLatin1Char('\n'), 0, 0))));
        }
        QVERIFY2(presetCards >= 4,
                 "expected the four preset cards (Screen/Ebook/Printer/Custom) to be clickable");
    }

    // Whatever the widgets show, the options reaching the engine must HONOR
    // them: subsetFonts reaches the estimator exactly as checked (the pass is
    // implemented; its estimator claims savings only through the same
    // eligibility walk the write path runs).
    void estimateOptionsHonorCheckbox() {
        auto engine = std::make_shared<RecordingEditorEngine>();
        AppContext ctx;
        ctx.pdfEditor = engine;

        gp::CompressDialog dlg(&ctx);  // constructor applies the Ebook preset → estimate runs
        QVERIFY2(engine->sawEstimate, "construction must trigger the live estimate");

        auto *subset = findBox(dlg, QStringLiteral("Subset fonts"));
        QVERIFY2(subset, "the 'Subset fonts' checkbox is missing");

        // Opted-out: the estimate must not claim subset savings.
        subset->setChecked(true);
        subset->setChecked(false);
        QVERIFY2(!engine->lastEstimateOpts.subsetFonts,
                 "the dialog asked the engine to subset fonts while the "
                 "checkbox was unchecked");

        // Opted-in: the engine must be told to run the pass.
        subset->setChecked(true);
        QVERIFY2(engine->lastEstimateOpts.subsetFonts,
                 "the dialog did not honor the checked 'Subset fonts' — the "
                 "pass is implemented and the user opted in");
        QVERIFY2(engine->lastEstimateOpts.removeUnusedObjects,
                 "the dialog must honor the checked 'Remove unused objects' — "
                 "the sweep is implemented and the checkbox defaults checked");
    }

    // The size figures shown live are predictions, not measurements; the row
    // must stay labeled as an estimate.
    void sizeRowStaysLabeledAsEstimate() {
        gp::CompressDialog dlg(nullptr);
        bool foundEstimateLabel = false;
        const auto labels = dlg.findChildren<QLabel*>();
        for (auto *l : labels) {
            if (l->text().contains(QStringLiteral("ESTIMAT"), Qt::CaseInsensitive)) {
                foundEstimateLabel = true;
                break;
            }
        }
        QVERIFY2(foundEstimateLabel,
                 "the predicted size row must be labeled as an estimate");
    }

    // ── §9.13: post-compression completion report is MEASURED ────────────────
    // After a successful optimizeDocument the completion message must report
    // the MEASURED sizes (both read from disk after the write), the delta,
    // and stay clearly distinct from the pre-execution estimate row.

    // A genuine reduction: both byte figures plus the saved-delta appear, the
    // result is labeled "(measured)", and no not-smaller caveat is shown.
    void completionReportCarriesMeasuredFiguresAndDelta() {
        const QString msg = gp::CompressDialog::formatCompletionReport(
            1536, 512, QStringLiteral("report_out.pdf"));

        qDebug() << "completion report (smaller):" << msg;

        QVERIFY2(msg.contains(QStringLiteral("Saved to: report_out.pdf")),
                 qPrintable(QStringLiteral("missing output name: %1").arg(msg)));
        QVERIFY2(msg.contains(QStringLiteral("Original size: 1.5 KB")),
                 qPrintable(QStringLiteral("missing measured original: %1").arg(msg)));
        QVERIFY2(msg.contains(QStringLiteral("New size: 512 B (measured)")),
                 qPrintable(QStringLiteral("missing measured result: %1").arg(msg)));
        // Delta: 1536 - 512 = 1024 bytes saved = 66.7% of the original.
        QVERIFY2(msg.contains(QStringLiteral("1.0 KB")),
                 qPrintable(QStringLiteral("missing measured delta: %1").arg(msg)));
        QVERIFY2(msg.contains(QStringLiteral("66.7%")),
                 qPrintable(QStringLiteral("missing measured reduction percent: %1").arg(msg)));
        QVERIFY2(!msg.contains(QStringLiteral("did not reduce"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("a real reduction must not carry the "
                                          "not-smaller caveat: %1").arg(msg)));
    }

    // R12 precedent at the same site: when the measured result is equal to or
    // LARGER than the original, the report says so honestly instead of
    // spinning a reduction that did not happen — and never prints a
    // "size reduction" line with a non-positive delta.
    void completionReportNotesWhenNotSmallerOrEqual() {
        // Equal sizes.
        const QString equal = gp::CompressDialog::formatCompletionReport(
            2048, 2048, QStringLiteral("equal.pdf"));
        qDebug() << "completion report (equal):" << equal;
        QVERIFY2(equal.contains(
                     QStringLiteral("compression did not reduce this document's size")),
                 qPrintable(QStringLiteral("equal result must carry the honesty note: %1")
                                .arg(equal)));
        QVERIFY2(!equal.contains(QStringLiteral("Size reduction"), Qt::CaseInsensitive),
                 qPrintable("an equal result must not claim a size reduction"));

        // Larger than the input (e.g. re-encoded JPEGs can grow the file).
        const QString larger = gp::CompressDialog::formatCompletionReport(
            1024, 4096, QStringLiteral("larger.pdf"));
        qDebug() << "completion report (larger):" << larger;
        QVERIFY2(larger.contains(QStringLiteral("New size: 4.0 KB (measured)")),
                 qPrintable(QStringLiteral("missing measured larger result: %1").arg(larger)));
        QVERIFY2(larger.contains(
                     QStringLiteral("compression did not reduce this document's size")),
                 qPrintable(QStringLiteral("larger result must carry the honesty note: %1")
                                .arg(larger)));
        QVERIFY2(!larger.contains(QStringLiteral("Size reduction"), Qt::CaseInsensitive),
                 qPrintable("a larger result must not claim a size reduction"));
    }

    // Estimate-vs-measured labeling stays distinct: the completion message is
    // explicitly "(measured)" and never borrows the estimate vocabulary the
    // pre-execution row uses.
    void completionReportStaysDistinctFromEstimateLabeling() {
        const QString msg = gp::CompressDialog::formatCompletionReport(
            1536, 512, QStringLiteral("distinct.pdf"));
        QVERIFY2(msg.contains(QStringLiteral("measured"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("completion must be labeled measured: %1").arg(msg)));
        QVERIFY2(!msg.contains(QStringLiteral("ESTIMAT"), Qt::CaseInsensitive)
                     && !msg.contains(QStringLiteral("estimat"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("completion message must not use the "
                                          "estimate vocabulary: %1").arg(msg)));
    }
};

QTEST_MAIN(TestCompressDialogHonesty)
#include "TestCompressDialogHonesty.moc"
