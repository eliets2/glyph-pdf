// SPDX-License-Identifier: Apache-2.0
// §9.4 regression test: the OCR preprocessing checkboxes (Deskew / Binarize /
// Denoise) are persisted prefs consumed by the OCR pipeline — previously they
// were dead UI (created, never connected), so the panel silently disagreed
// with the pipeline (checkbox said "off", pipeline denoised anyway).
#include <QtTest/QtTest>
#include <QSettings>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFileInfo>

#include "modes/OCRMode.h"

class TestOcrPreprocessPrefs : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        // Isolate QSettings: never read the user's real prefs, never clobber
        // them (same idiom as TestBatchOcrLanguage).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestOcrPreprocessPrefs"));
    }

    void cleanup() {
        QSettings().remove(QStringLiteral("ocr/preprocessDeskew"));
        QSettings().remove(QStringLiteral("ocr/preprocessBinarize"));
        QSettings().remove(QStringLiteral("ocr/preprocessDenoise"));
    }

    void defaultsMatchPipelineBehavior() {
        gp::OCRMode mode;
        auto* deskew = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDeskew"));
        auto* binarize = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkBinarize"));
        auto* denoise = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDenoise"));
        QVERIFY(deskew && binarize && denoise);
        // F5-F2 (SWEEP-W3-UX): the shipped defaults are HONEST and SAFE — the
        // destructive chain (deskew/binarize/denoise) is OFF out of the box so
        // recognition works on a clean scan without the user asking for it
        // (the audit observed the old on-by-default chain zero recognition).
        // The panel must tell the truth about that.
        QVERIFY2(!deskew->isChecked(), "shipped Deskew default must be off "
                                       "(destructive chain is opt-in)");
        QVERIFY2(!binarize->isChecked(), "shipped Binarize default must be off "
                                         "(destructive chain is opt-in)");
        QVERIFY2(!denoise->isChecked(),
                 "the shipped Denoise default must be off — and the checkbox "
                 "must match the pipeline's actual default (it used to show "
                 "off while the pipeline denoised anyway; now BOTH are off "
                 "until the user opts in)");
    }

    void togglingPersistsToQSettings() {
        // CX-17: the old version of this slot toggled false→false, so no
        // toggled signal ever fired and the "persisted" assertion ran against
        // an ABSENT key — toBool() on an absent key reads as false, so the
        // test passed even with the writer disconnected (negative control:
        // docs/audit/evidence-cx17/old-test-writer-disconnected-STILL-PASSES.txt).
        // The real persistence cycle per key: clear → toggle false→true → the
        // key must EXIST and hold true → reconstruct → still true → toggle
        // back off → the key must exist and hold false.
        const struct { const char *box; const char *key; } cases[] = {
            { "ocrChkDeskew",    "ocr/preprocessDeskew" },
            { "ocrChkBinarize",  "ocr/preprocessBinarize" },
            { "ocrChkDenoise",   "ocr/preprocessDenoise" },
        };
        for (const auto &c : cases) {
            const QString box = QString::fromLatin1(c.box);
            const QString key = QString::fromLatin1(c.key);
            QSettings().remove(key);

            {
                gp::OCRMode mode;
                auto* chk = mode.findChild<QCheckBox*>(box);
                QVERIFY2(chk, qPrintable(QStringLiteral("missing checkbox: ") + box));
                QVERIFY2(!chk->isChecked(),
                         "with the key cleared the checkbox must start off");
                chk->setChecked(true);        // fires toggled(true) → the writer
            }
            {
                QSettings after;
                QVERIFY2(after.contains(key),
                         "toggling on must WRITE the key — an absent key here "
                         "means the panel no longer persists (the CX-17 bug "
                         "shape); the old test could not catch this");
                QCOMPARE(after.value(key).toBool(), true);
            }
            {
                gp::OCRMode mode;
                auto* chk = mode.findChild<QCheckBox*>(box);
                QVERIFY2(chk && chk->isChecked(),
                         "a persisted true must restore as checked");
                chk->setChecked(false);       // toggle back off
            }
            {
                QSettings after;
                QVERIFY2(after.contains(key),
                         "toggling off must keep the key present (an explicit "
                         "off, not an absent default)");
                QCOMPARE(after.value(key).toBool(), false);
            }
        }
    }

    void persistedPrefsRestoreOnConstruction() {
        // CX-17: the old version stored FALSE — the constructor default — so
        // a constructor that ignored saved values entirely still passed. The
        // restore path is only proven by a value the default disagrees with:
        // store true behind the panel's back, reconstruct, and require the
        // checkbox to come up CHECKED.
        QSettings().setValue(QStringLiteral("ocr/preprocessDeskew"), true);
        QSettings().setValue(QStringLiteral("ocr/preprocessDenoise"), true);
        gp::OCRMode mode;
        auto* deskew = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDeskew"));
        auto* denoise = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDenoise"));
        QVERIFY2(deskew && deskew->isChecked(),
                 "a persisted 'on' must restore as on — the pref is what the "
                 "pipeline will honor");
        QVERIFY2(denoise && denoise->isChecked(),
                 "persisted Denoise=on must restore checked");
    }

    void firstRunOcrSeedIsStagedBesideTheBuild() {
        // PROGRAM-CONSOLIDATION-2026-09-25 §1.7a: first-run OCR is zero-network
        // by design — the policy download gate is off in tests — so the only
        // seed OcrEngine::initialize may use is applicationDirPath()/tessdata
        // (the location the MSI ships, packaging/deploy.ps1), copied into the
        // strict AppLocalData path on first use. The build therefore stages
        // eng.traineddata beside the test executables exactly like the models/
        // staging; when that staging is missing, every pristine machine (CI
        // runner, fresh clone) fails first-run OCR with "Tesseract language
        // data for 'eng' is unavailable" and TestSweepW3UxFlows flow5 burns
        // its whole 250s recognition budget (CI run 36538793928). This pin
        // makes a missing staging fail fast, everywhere, with the reason.
        const QString staged =
            QCoreApplication::applicationDirPath()
            + QStringLiteral("/tessdata/eng.traineddata");
        QVERIFY2(QFileInfo::exists(staged),
                 qPrintable(QStringLiteral("the first-run OCR seed must be staged at %1 "
                                          "(CMake GLYPHPDF_TESSDATA_DIR staging) — without it "
                                          "first-run OCR honestly fails with 'Tesseract "
                                          "language data for eng is unavailable' on any "
                                          "pristine machine").arg(staged)));
    }
};

QTEST_MAIN(TestOcrPreprocessPrefs)
#include "TestOcrPreprocessPrefs.moc"
