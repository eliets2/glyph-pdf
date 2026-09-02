// SPDX-License-Identifier: Apache-2.0
// §9.4 regression test: the OCR preprocessing checkboxes (Deskew / Binarize /
// Denoise) are persisted prefs consumed by the OCR pipeline — previously they
// were dead UI (created, never connected), so the panel silently disagreed
// with the pipeline (checkbox said "off", pipeline denoised anyway).
#include <QtTest/QtTest>
#include <QSettings>
#include <QCheckBox>

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
        // OcrPreprocessOptions has long defaulted deskew/binarize/denoise to
        // true — the panel must tell the truth about that.
        QVERIFY(deskew->isChecked());
        QVERIFY(binarize->isChecked());
        QVERIFY2(denoise->isChecked(),
                 "the Denoise checkbox must match the pipeline's actual default "
                 "(it used to show off while the pipeline denoised anyway)");
    }

    void togglingPersistsToQSettings() {
        {
            gp::OCRMode mode;
            auto* denoise = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDenoise"));
            QVERIFY(denoise);
            denoise->setChecked(false);
        }
        QCOMPARE(QSettings().value(QStringLiteral("ocr/preprocessDenoise")).toBool(), false);

        {
            gp::OCRMode mode;
            auto* deskew = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDeskew"));
            QVERIFY(deskew);
            deskew->setChecked(false);
        }
        QCOMPARE(QSettings().value(QStringLiteral("ocr/preprocessDeskew")).toBool(), false);
    }

    void persistedPrefsRestoreOnConstruction() {
        QSettings().setValue(QStringLiteral("ocr/preprocessDeskew"), false);
        QSettings().setValue(QStringLiteral("ocr/preprocessDenoise"), false);
        gp::OCRMode mode;
        auto* deskew = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDeskew"));
        auto* denoise = mode.findChild<QCheckBox*>(QStringLiteral("ocrChkDenoise"));
        QVERIFY2(deskew && !deskew->isChecked(),
                 "a persisted 'off' must restore as off — the pref is what the "
                 "pipeline will honor");
        QVERIFY2(denoise && !denoise->isChecked(), "persisted Denoise=off must restore");
    }
};

QTEST_MAIN(TestOcrPreprocessPrefs)
#include "TestOcrPreprocessPrefs.moc"
