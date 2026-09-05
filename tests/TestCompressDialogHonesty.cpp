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
};

QTEST_MAIN(TestCompressDialogHonesty)
#include "TestCompressDialogHonesty.moc"
