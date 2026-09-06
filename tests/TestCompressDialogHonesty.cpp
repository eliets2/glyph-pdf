// SPDX-License-Identifier: Apache-2.0
// R12 regression test: "Subset fonts" and "Remove unused objects" shipped
// CHECKED in CompressDialog while the compression backend implements neither
// pass (no font subsetter, no object garbage collector). A user could select —
// and the estimate could promise savings for — passes that never run.
//
// This test pins the UI-honesty contract:
//   1. the availability explanation (unsupportedPassExplanation) is honest,
//   2. both checkboxes still exist (the promise is removed, not the control)
//      but are DISABLED, UNCHECKED, and carry the explanation,
//   3. no preset re-enables or re-checks them,
//   4. the OptimizeOptions the dialog hands to the engine never request
//      either unsupported pass,
//   5. the size row stays explicitly labeled as an estimate.
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
        v << QStringLiteral("the 'Subset fonts' checkbox is missing (R12 removes the promise, not the control)");
    if (!remove)
        v << QStringLiteral("the 'Remove unused objects' checkbox is missing");
    if (subset) {
        if (subset->isEnabled())
            v << QStringLiteral("'Subset fonts' is ENABLED but the pass is not implemented in this build");
        if (subset->isChecked())
            v << QStringLiteral("'Subset fonts' is CHECKED but the engine never subsets fonts");
        if (subset->toolTip() != gp::CompressDialog::unsupportedPassExplanation())
            v << QStringLiteral("'Subset fonts' tooltip does not carry the availability explanation");
        if (subset->statusTip() != gp::CompressDialog::unsupportedPassExplanation())
            v << QStringLiteral("'Subset fonts' status tip does not carry the availability explanation");
    }
    if (remove) {
        if (remove->isEnabled())
            v << QStringLiteral("'Remove unused objects' is ENABLED but the pass is not implemented in this build");
        if (remove->isChecked())
            v << QStringLiteral("'Remove unused objects' is CHECKED but the engine never removes unused objects");
        if (remove->toolTip() != gp::CompressDialog::unsupportedPassExplanation())
            v << QStringLiteral("'Remove unused objects' tooltip does not carry the availability explanation");
        if (remove->statusTip() != gp::CompressDialog::unsupportedPassExplanation())
            v << QStringLiteral("'Remove unused objects' status tip does not carry the availability explanation");
    }
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

    // The seam is the single source of truth for the availability text the
    // disabled checkboxes surface; it must actually say the passes are not
    // implemented/available rather than hinting at a tuning problem.
    void unsupportedPassExplanationIsHonest() {
        const QString text = gp::CompressDialog::unsupportedPassExplanation();
        QVERIFY2(!text.trimmed().isEmpty(),
                 "unsupportedPassExplanation() must not be empty");
        QVERIFY2(text.contains(QStringLiteral("not implemented"), Qt::CaseInsensitive)
                 || text.contains(QStringLiteral("not available"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral(
                     "the explanation must state the passes are not implemented/available "
                     "in this build; got: %1").arg(text)));
    }

    void unsupportedPassesAreDisabledUncheckedWithExplanation() {
        gp::CompressDialog dlg(nullptr);
        const QStringList v = stateViolations(dlg);
        QVERIFY2(v.isEmpty(), qPrintable(v.join(QStringLiteral("; "))));
    }

    // Switching presets (Screen/Ebook/Printer/Custom) used to re-check both
    // unsupported passes; no preset may re-enable or re-check them.
    void presetsNeverReEnableOrReCheckUnsupportedPasses() {
        gp::CompressDialog dlg(nullptr);

        int presetCards = 0;
        const auto buttons = dlg.findChildren<QToolButton*>();
        for (auto *b : buttons) {
            if (!b->isCheckable())
                continue;
            ++presetCards;
            b->click();
            const QStringList v = stateViolations(dlg);
            QVERIFY2(v.isEmpty(),
                     qPrintable(QStringLiteral("after clicking preset '%1': %2")
                                    .arg(b->text().section(QLatin1Char('\n'), 0, 0),
                                         v.join(QStringLiteral("; ")))));
        }
        QVERIFY2(presetCards >= 4,
                 "expected the four preset cards (Screen/Ebook/Printer/Custom) to be clickable");
    }

    // Whatever the widgets show, the options reaching the engine must not
    // request the unimplemented passes (OptimizeOptions defaults both to true,
    // so the dialog has to pin them off explicitly).
    void estimateOptionsNeverRequestUnsupportedPasses() {
        auto engine = std::make_shared<RecordingEditorEngine>();
        AppContext ctx;
        ctx.pdfEditor = engine;

        gp::CompressDialog dlg(&ctx);  // constructor applies the Ebook preset → estimate runs
        QVERIFY2(engine->sawEstimate, "construction must trigger the live estimate");

        const auto buttons = dlg.findChildren<QToolButton*>();
        for (auto *b : buttons) {
            if (b->isCheckable())
                b->click();
        }

        QVERIFY2(!engine->lastEstimateOpts.subsetFonts,
                 "the dialog asked the engine to subset fonts — a pass the "
                 "backend does not implement");
        QVERIFY2(!engine->lastEstimateOpts.removeUnusedObjects,
                 "the dialog asked the engine to remove unused objects — a pass "
                 "the backend does not implement");
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
