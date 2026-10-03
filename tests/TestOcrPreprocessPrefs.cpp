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
#include "engines/ocr/OcrPreprocessor.h"
#include "shell/controllers/EditController.h"

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
        QSettings().remove(QStringLiteral("ocr/orientDetect"));
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

    // ── row 17, consumption half (findings-tests 2026-10-02, testing-
    // specialist H-2): "the pipeline CONSUMES the persisted prefs". The pins
    // above prove checkbox → QSettings; EditController::runOcrRegion reads
    // the SAME keys through ocrPreprocessPrefsFromSettings() and feeds them
    // to OcrPipeline::setPreprocessing — and that consumption mapping was
    // asserted by zero tests repo-wide: deleting, retyping or mis-defaulting
    // the reads left every pin here green (the historical bug's exact
    // shape: checkbox said off, pipeline denoised anyway).
    void pipelineConsumesPersistedPrefs() {
        // Absent keys → the shipped defaults, ALL FOUR false (F5-F2: the
        // destructive chain is opt-in; Auto-Rotate too). The keys are
        // actually REMOVED first, never merely assumed absent (CX-17).
        QSettings().remove(QStringLiteral("ocr/preprocessDeskew"));
        QSettings().remove(QStringLiteral("ocr/preprocessBinarize"));
        QSettings().remove(QStringLiteral("ocr/preprocessDenoise"));
        QSettings().remove(QStringLiteral("ocr/orientDetect"));
        {
            const auto prefs = gp::EditController::ocrPreprocessPrefsFromSettings();
            QVERIFY2(!prefs.deskew && !prefs.binarize && !prefs.denoise
                         && !prefs.orientDetect,
                     "with every pref absent the pipeline's preprocessing must be "
                     "the shipped all-OFF default — an ON default here denoises "
                     "behind the user's back on a clean scan (the audited "
                     "zero-recognition failure)");
        }
        // Each persisted true reaches EXACTLY its matching field — and never
        // a sibling (the retyped/wrong-key regression class).
        QSettings().setValue(QStringLiteral("ocr/preprocessDeskew"), true);
        {
            const auto prefs = gp::EditController::ocrPreprocessPrefsFromSettings();
            QVERIFY2(prefs.deskew, "persisted Deskew=on must reach the pipeline");
            QVERIFY2(!prefs.binarize && !prefs.denoise && !prefs.orientDetect,
                     "the Deskew pref must not leak into a sibling field");
        }
        QSettings().setValue(QStringLiteral("ocr/preprocessBinarize"), true);
        {
            const auto prefs = gp::EditController::ocrPreprocessPrefsFromSettings();
            QVERIFY2(prefs.deskew && prefs.binarize,
                     "persisted Binarize=on must reach the pipeline");
            QVERIFY2(!prefs.denoise && !prefs.orientDetect,
                     "the Binarize pref must not leak into a sibling field");
        }
        QSettings().setValue(QStringLiteral("ocr/preprocessDenoise"), true);
        {
            const auto prefs = gp::EditController::ocrPreprocessPrefsFromSettings();
            QVERIFY2(prefs.deskew && prefs.binarize && prefs.denoise,
                     "persisted Denoise=on must reach the pipeline (the checkbox "
                     "said off while the pipeline denoised — the historical bug)");
            QVERIFY2(!prefs.orientDetect,
                     "the Denoise pref must not leak into Auto-Rotate");
        }
        QSettings().setValue(QStringLiteral("ocr/orientDetect"), true);
        {
            const auto prefs = gp::EditController::ocrPreprocessPrefsFromSettings();
            QVERIFY2(prefs.orientDetect,
                     "persisted Auto-Rotate=on must reach the pipeline");
        }
        // An explicit false survives as false (an absent key and an explicit
        // off must behave identically at the pipeline).
        QSettings().setValue(QStringLiteral("ocr/preprocessDenoise"), false);
        QCOMPARE(gp::EditController::ocrPreprocessPrefsFromSettings().denoise, false);
    }


    // ── §4 row 17 (parity row 22): preprocessing capability disclosure ──────
    // A build without Leptonica silently degraded preprocessing: deskew and
    // Auto-Rotate became no-ops, Binarize dropped to a fixed threshold, and
    // the HAS_TESSERACT gating was invisible in the UI. The preprocessing
    // checkboxes' tooltips must therefore be composed from the
    // OcrPreprocessor::leptonicaAvailable() capability query — never a
    // hardcoded string — and disclose plainly in BOTH configurations:
    //  - without Leptonica: Deskew/Auto-Rotate say they are not available and
    //    what to do instead; Binarize discloses the fixed-threshold fallback;
    //  - with Leptonica: no false absence/degradation claims either.
    // Runs in both configurations on purpose (D03 dual-config discipline):
    // each branch is only exercised where its wording is the truth.
    void tooltipsDisclosePreprocessingCapability() {
        gp::OCRMode mode;
        auto* deskew   = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDeskew"));
        auto* binarize = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkBinarize"));
        auto* denoise  = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDenoise"));
        auto* orient   = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkOrientDetect"));
        QVERIFY2(deskew && binarize && denoise && orient,
                 "all four preprocessing checkboxes must exist");

        const QString deskewTip   = deskew->toolTip();
        const QString binarizeTip = binarize->toolTip();
        const QString denoiseTip  = denoise->toolTip();
        const QString orientTip   = orient->toolTip();

        // A SILENT checkbox is exactly the row-22 failure shape: every
        // preprocessing toggle must carry SOME capability disclosure.
        QVERIFY2(!deskewTip.isEmpty(),   "Deskew checkbox must have a tooltip");
        QVERIFY2(!binarizeTip.isEmpty(), "Binarize checkbox must have a tooltip");
        QVERIFY2(!denoiseTip.isEmpty(),  "Denoise checkbox must have a tooltip");
        QVERIFY2(!orientTip.isEmpty(),   "Auto-Rotate checkbox must have a tooltip");

        // Denoise is Qt-only: it works in every build, so its tooltip must
        // never claim the capability is missing (in either configuration).
        QVERIFY2(!denoiseTip.contains(QStringLiteral("not available"),
                                      Qt::CaseInsensitive),
                 "Denoise works in every build — its tooltip must not claim "
                 "it is unavailable");

        const QString absence = QStringLiteral("not available in this build");
        if (!OcrPreprocessor::leptonicaAvailable()) {
            QVERIFY2(deskewTip.contains(absence, Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("no-Leptonica build: the Deskew tooltip "
                                              "must disclose the missing capability; got: ")
                                + deskewTip));
            QVERIFY2(orientTip.contains(absence, Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("no-Leptonica build: the Auto-Rotate tooltip "
                                              "must disclose the missing capability; got: ")
                                + orientTip));
            QVERIFY2(binarizeTip.contains(QStringLiteral("fixed threshold"),
                                          Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("no-Leptonica build: the Binarize tooltip "
                                              "must disclose the fixed-threshold fallback; got: ")
                                + binarizeTip));
        } else {
            // An honest Leptonica build must not scare users with absence
            // wording that is not true of it (the hardcoded-string failure
            // shape: a tooltip that ignores the capability query).
            QVERIFY2(!deskewTip.contains(absence, Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("Leptonica build: the Deskew tooltip must not "
                                              "claim the capability is missing; got: ")
                                + deskewTip));
            QVERIFY2(!orientTip.contains(absence, Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("Leptonica build: the Auto-Rotate tooltip must not "
                                              "claim the capability is missing; got: ")
                                + orientTip));
            QVERIFY2(!binarizeTip.contains(QStringLiteral("fixed threshold"),
                                           Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("Leptonica build: the Binarize tooltip must not "
                                              "claim the degraded fallback; got: ")
                                + binarizeTip));
        }
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
