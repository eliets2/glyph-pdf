// SPDX-License-Identifier: Apache-2.0
// U08 regression test: ONE CapabilityRegistry answers "available? why not?
// what alternative?" for every workflow, BEFORE work runs.
//
// Contracts pinned here:
//   1. query() caches per (id, param); invalidate()/invalidateAll() re-probe;
//      capabilityChanged fires on invalidate.
//   2. UnavailableBuild (compile-time floor) and UnavailableRuntime (runtime
//      probe) are distinct states — the registry never coerces one into the
//      other (the old HTML-as-.docx bug class).
//   3. EVERY Unavailable*/Degraded result carries a non-empty whyNot AND a
//      non-empty alternative — lazy providers get a generic fallback plus a
//      qWarning so they are visible in test logs.
//   4. applyToWidget() disables the widget and surfaces whyNot + alternative
//      (generalizes CompressDialog's disabled+tooltip idiom); Degraded keeps
//      the widget enabled and discloses via tooltip only.
//   5. fileFilterFor() contributes only clauses whose capability is
//      Available/Degraded, preserving caller order.
//   6. The engine probes registered by registerEngineProbes() tell the truth:
//      OfficeImport matches ConversionManager::isOfficeImportAvailable();
//      Word/Excel export are available in EVERY build (in-house OOXML
//      writers); the R12 compression passes are UnavailableBuild with the
//      exact wording CompressDialog surfaces; OCR language data is
//      Available/Degraded (downloadable) for supported languages and
//      UnavailableRuntime otherwise.
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QWidget>

#include "core/Capability.h"
#include "engines/ConversionManager.h"

using gp::Availability;
using gp::Capability;
using gp::CapabilityRegistry;
using gp::CapId;

namespace {

// Probe that reports how often it was invoked — the cache seam.
CapabilityRegistry::Probe countingProbe(int* counter,
                                        Availability status = Availability::Available)
{
    return [counter, status](const QVariant&) {
        ++*counter;
        Capability c;
        c.status = status;
        c.whyNot     = QStringLiteral("counting whyNot");
        c.alternative = QStringLiteral("counting alternative");
        c.detail     = QStringLiteral("counting detail");
        return c;
    };
}

// A lazy probe: returns an unavailable/degraded result with NO explanation.
Capability lazyProbe(const QVariant&)
{
    Capability c;
    c.status = Availability::UnavailableRuntime;
    return c;
}

} // namespace

class TestCapabilityRegistry : public QObject {
    Q_OBJECT

private slots:

    // ── 1. Cache / invalidate ────────────────────────────────────────────────

    void queryCachesUntilInvalidate() {
        int calls = 0;
        CapabilityRegistry reg;
        reg.registerProbe(CapId::OfficeImport, countingProbe(&calls));

        (void)reg.query(CapId::OfficeImport);
        (void)reg.query(CapId::OfficeImport);
        (void)reg.available(CapId::OfficeImport);
        QCOMPARE(calls, 1);                       // cached — one probe run

        reg.invalidate(CapId::OfficeImport);
        (void)reg.query(CapId::OfficeImport);
        QCOMPARE(calls, 2);                       // invalidate forces a re-probe
    }

    void parameterizedQueriesCachePerParam() {
        int calls = 0;
        CapabilityRegistry reg;
        reg.registerProbe(CapId::OcrLanguageData, countingProbe(&calls));

        (void)reg.query(CapId::OcrLanguageData, QStringLiteral("EN"));
        (void)reg.query(CapId::OcrLanguageData, QStringLiteral("EN"));
        QCOMPARE(calls, 1);                       // same param → cached

        (void)reg.query(CapId::OcrLanguageData, QStringLiteral("DE"));
        QCOMPARE(calls, 2);                       // different param → separate probe

        reg.invalidateAll();
        (void)reg.query(CapId::OcrLanguageData, QStringLiteral("EN"));
        (void)reg.query(CapId::OcrLanguageData, QStringLiteral("DE"));
        QCOMPARE(calls, 4);                       // invalidateAll clears every entry
    }

    void capabilityChangedFiresOnInvalidate() {
        int calls = 0;
        CapabilityRegistry reg;
        reg.registerProbe(CapId::OfficeImport, countingProbe(&calls));
        reg.registerProbe(CapId::WordExport, countingProbe(&calls));

        QSignalSpy spy(&reg, &CapabilityRegistry::capabilityChanged);
        QVERIFY(spy.isValid());

        reg.invalidate(CapId::OfficeImport);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().value<CapId>(), CapId::OfficeImport);

        reg.invalidateAll();
        QCOMPARE(spy.count(), 3);                 // one per registered capability
    }

    // ── 2. UnavailableBuild vs UnavailableRuntime; non-empty rule ────────────

    void buildAndRuntimeDistinctionIsPreserved() {
        CapabilityRegistry reg;
        reg.registerProbe(CapId::CompressSubsetFonts, [](const QVariant&) {
            Capability c;
            c.status = Availability::UnavailableBuild;
            c.whyNot = QStringLiteral("not in this build");
            c.alternative = QStringLiteral("do something else");
            return c;
        });
        reg.registerProbe(CapId::OcrRapidModels, [](const QVariant&) {
            Capability c;
            c.status = Availability::UnavailableRuntime;
            c.whyNot = QStringLiteral("models not installed");
            c.alternative = QStringLiteral("install them");
            return c;
        });

        // The registry must never coerce the compile-time floor into a runtime
        // state or vice versa — the caller can trust the distinction.
        QCOMPARE(reg.query(CapId::CompressSubsetFonts).status, Availability::UnavailableBuild);
        QCOMPARE(reg.query(CapId::OcrRapidModels).status, Availability::UnavailableRuntime);

        QVERIFY(!reg.available(CapId::CompressSubsetFonts));
        QVERIFY(!reg.available(CapId::OcrRapidModels));
    }

    void unavailableResultsCarryWhyNotAndAlternative() {
        CapabilityRegistry reg;

        // A provider that explains itself keeps its wording verbatim.
        reg.registerProbe(CapId::OcrRapidModels, [](const QVariant&) {
            Capability c;
            c.status = Availability::Degraded;
            c.whyNot = QStringLiteral("honest whyNot");
            c.alternative = QStringLiteral("honest alternative");
            return c;
        });
        const Capability honest = reg.query(CapId::OcrRapidModels);
        QCOMPARE(honest.whyNot, QStringLiteral("honest whyNot"));
        QCOMPARE(honest.alternative, QStringLiteral("honest alternative"));

        // A lazy provider (empty whyNot/alternative) must still yield non-empty
        // strings — applyToWidget/tooltip consumers can never render nothing.
        reg.registerProbe(CapId::Linearize, lazyProbe);
        const Capability lazy = reg.query(CapId::Linearize);
        QVERIFY2(!lazy.whyNot.trimmed().isEmpty(),
                 "an Unavailable result must never carry an empty whyNot");
        QVERIFY2(!lazy.alternative.trimmed().isEmpty(),
                 "an Unavailable result must never carry an empty alternative");
    }

    void missingProbeStillExplainsItself() {
        CapabilityRegistry reg;   // nothing registered
        const Capability c = reg.query(CapId::PdfAValidation);
        QVERIFY(!reg.available(CapId::PdfAValidation));
        QVERIFY2(!c.whyNot.trimmed().isEmpty(),
                 "even an unregistered capability must explain itself");
        QVERIFY2(!c.alternative.trimmed().isEmpty(),
                 "even an unregistered capability must offer an alternative");
    }

    // ── 3. applyToWidget (generalizes CompressDialog's idiom) ────────────────

    void applyToWidgetDisablesUnavailableWithExplanation() {
        CapabilityRegistry reg;
        reg.registerProbe(CapId::PdfAValidation, [](const QVariant&) {
            Capability c;
            c.status = Availability::UnavailableRuntime;
            c.whyNot = QStringLiteral("the validator is missing");
            c.alternative = QStringLiteral("install the validator");
            return c;
        });

        QWidget w;
        reg.applyToWidget(&w, CapId::PdfAValidation);
        QVERIFY2(!w.isEnabled(), "an Unavailable capability must disable the widget");
        const QString expected = QStringLiteral("the validator is missing install the validator");
        QVERIFY2(w.toolTip() == expected,
                 qPrintable(QStringLiteral("tooltip must carry whyNot + alternative; got '%1'")
                                    .arg(w.toolTip())));
        QVERIFY2(w.statusTip() == expected, "statusTip must carry whyNot + alternative");

        // Idempotent — re-applying does not stack the explanation.
        reg.applyToWidget(&w, CapId::PdfAValidation);
        QVERIFY2(w.toolTip() == expected, "re-applying must not duplicate the tooltip");
    }

    // D06 residual (wave 4A): the OWNED-disable reversal. The unavailable →
    // invalidate → available transition must re-enable the widget and clear
    // the owned explanation — while a widget the registry never disabled is
    // never touched by the Available branch.
    void applyToWidgetOwnedDisableIsReversedWhenCapabilityBecomesAvailable() {
        bool available = false;
        CapabilityRegistry reg;
        reg.registerProbe(CapId::PdfAValidation, [&](const QVariant&) {
            Capability c;
            c.status = available ? Availability::Available
                                 : Availability::UnavailableRuntime;
            if (!available) {
                c.whyNot = QStringLiteral("the validator is missing");
                c.alternative = QStringLiteral("install the validator");
            }
            return c;
        });

        QWidget w;
        reg.applyToWidget(&w, CapId::PdfAValidation);
        QVERIFY2(!w.isEnabled(), "unavailable must disable the widget");

        // Models "the validator was installed mid-session": invalidate + apply.
        available = true;
        reg.invalidate(CapId::PdfAValidation);
        reg.applyToWidget(&w, CapId::PdfAValidation);
        QVERIFY2(w.isEnabled(),
                 "the registry's own disable must be reversed once Available");
        QVERIFY2(w.toolTip().isEmpty(),
                 "the owned explanation must be cleared once Available");
        QVERIFY2(w.statusTip().isEmpty(),
                 "the owned statusTip must be cleared once Available");

        // A widget the registry never disabled is NOT re-enabled by it —
        // only the owned disable (capOwnedDisable property) is undone.
        QWidget foreign;
        foreign.setEnabled(false);
        reg.applyToWidget(&foreign, CapId::PdfAValidation);
        QVERIFY2(!foreign.isEnabled(),
                 "the registry must not re-enable a disable it did not set");
    }

    // ── G11 (QUALITY-GATE-2026-09-09): disable ownership across the FULL
    // unavailable→available transition. The wave-4A control above stopped at
    // "unavailable keeps the foreign disable" — the reviewer's probe showed
    // the registry still RE-ENABLING the foreign-disabled widget once the
    // capability becomes Available, because it had claimed ownership of a
    // disable it never performed.
    void applyToWidgetPreservesForeignDisableAcrossUnavailableToAvailableTransition() {
        bool available = false;
        CapabilityRegistry reg;
        reg.registerProbe(CapId::PdfAValidation, [&](const QVariant&) {
            Capability c;
            c.status = available ? Availability::Available
                                 : Availability::UnavailableRuntime;
            if (!available) {
                c.whyNot = QStringLiteral("the validator is missing");
                c.alternative = QStringLiteral("install the validator");
            }
            return c;
        });

        // ANOTHER owner disabled this control before the registry ever saw it.
        QWidget foreign;
        foreign.setEnabled(false);

        // Unavailable: the registry must not claim the foreign disable…
        reg.applyToWidget(&foreign, CapId::PdfAValidation);
        QVERIFY2(!foreign.isEnabled(), "unavailable keeps the widget disabled");
        QVERIFY2(!foreign.property("capOwnedDisable").toBool(),
                 "the registry must NOT claim ownership of a foreign disable");

        // …so Available (after invalidate) must leave the foreign state alone.
        available = true;
        reg.invalidate(CapId::PdfAValidation);
        reg.applyToWidget(&foreign, CapId::PdfAValidation);
        QVERIFY2(!foreign.isEnabled(),
                 "G11: the registry must not re-enable a widget another owner "
                 "disabled — across the unavailable→available transition");

        // Control: the registry's OWN disable still reverses on the same
        // transition (ownership and foreign-disable preservation coexist).
        QWidget owned;
        owned.setEnabled(true);
        reg.invalidate(CapId::PdfAValidation);
        available = false;
        reg.applyToWidget(&owned, CapId::PdfAValidation);
        QVERIFY2(!owned.isEnabled(), "unavailable disables the registry-owned widget");
        available = true;
        reg.invalidate(CapId::PdfAValidation);
        reg.applyToWidget(&owned, CapId::PdfAValidation);
        QVERIFY2(owned.isEnabled(),
                 "the registry's own disable must still reverse once Available");
    }

    // G11: an existing OWNED claim must survive repeated unavailable applies —
    // re-applying Unavailable sees the widget already disabled; the registry
    // still owns that state and must reverse it on a later Available.
    void applyToWidgetRetainsOwnedClaimAcrossRepeatedUnavailableApplies() {
        bool available = false;
        CapabilityRegistry reg;
        reg.registerProbe(CapId::PdfAValidation, [&](const QVariant&) {
            Capability c;
            c.status = available ? Availability::Available
                                 : Availability::UnavailableRuntime;
            if (!available) {
                c.whyNot = QStringLiteral("missing");
                c.alternative = QStringLiteral("install");
            }
            return c;
        });

        QWidget w;
        reg.applyToWidget(&w, CapId::PdfAValidation);   // claims (was enabled)
        reg.applyToWidget(&w, CapId::PdfAValidation);   // re-apply: keep the claim
        reg.applyToWidget(&w, CapId::PdfAValidation);
        available = true;
        reg.invalidate(CapId::PdfAValidation);
        reg.applyToWidget(&w, CapId::PdfAValidation);
        QVERIFY2(w.isEnabled(),
                 "an owned disable must survive repeated unavailable applies "
                 "and still reverse once Available");
    }

    void applyToWidgetIsNoOpForAvailableAndDegradedStaysEnabled() {
        CapabilityRegistry reg;
        reg.registerProbe(CapId::WordExport, [](const QVariant&) {
            Capability c;
            c.status = Availability::Available;
            return c;
        });
        reg.registerProbe(CapId::MrcCompression, [](const QVariant&) {
            Capability c;
            c.status = Availability::Degraded;
            c.whyNot = QStringLiteral("needs page images first");
            c.alternative = QStringLiteral("run OCR first");
            c.detail = QStringLiteral("degraded detail");
            return c;
        });

        QWidget ok;
        ok.setToolTip(QStringLiteral("keep me"));
        reg.applyToWidget(&ok, CapId::WordExport);
        QVERIFY(ok.isEnabled());
        QVERIFY2(ok.toolTip() == QStringLiteral("keep me"),
                 "an Available capability must not touch the widget");

        QWidget degraded;
        degraded.setEnabled(true);
        reg.applyToWidget(&degraded, CapId::MrcCompression);
        QVERIFY2(degraded.isEnabled(),
                 "a Degraded capability discloses but must not disable the widget");
        QVERIFY2(degraded.toolTip() == QStringLiteral("degraded detail"),
                 qPrintable(QStringLiteral("Degraded discloses via the detail tooltip; got '%1'")
                                    .arg(degraded.toolTip())));
    }

    // ── 4. fileFilterFor ─────────────────────────────────────────────────────

    void fileFilterForOmitsUnavailableKeepsOrder() {
        CapabilityRegistry reg;
        reg.registerProbe(CapId::WordExport, [](const QVariant&) {
            Capability c; c.status = Availability::Available; return c;
        });
        reg.registerProbe(CapId::PptExport, [](const QVariant&) {
            Capability c;
            c.status = Availability::UnavailableBuild;
            c.whyNot = QStringLiteral("no pptx in this build");
            c.alternative = QStringLiteral("use pdf");
            return c;
        });
        reg.registerProbe(CapId::MrcCompression, [](const QVariant&) {
            Capability c;
            c.status = Availability::Degraded;
            c.whyNot = QStringLiteral("d");
            c.alternative = QStringLiteral("a");
            return c;
        });

        const QList<QPair<QString, CapId>> clauses = {
            { QStringLiteral("Word Documents (*.docx)"), CapId::WordExport },
            { QStringLiteral("PowerPoint (*.pptx)"),     CapId::PptExport },
            { QStringLiteral("MRC PDF (*.pdf)"),         CapId::MrcCompression },
        };
        const QString filter = reg.fileFilterFor(clauses);

        QVERIFY2(!filter.contains(QStringLiteral("pptx")),
                 "an Unavailable capability must not contribute a filter clause");
        // Caller order is preserved.
        const int wordIdx = filter.indexOf(QStringLiteral("Word Documents"));
        const int mrcIdx  = filter.indexOf(QStringLiteral("MRC PDF"));
        QVERIFY2(wordIdx >= 0 && mrcIdx > wordIdx,
                 qPrintable(QStringLiteral("clauses keep caller order; got '%1'").arg(filter)));
        // Degraded still contributes (with a visible "limited" marker).
        QVERIFY2(filter.contains(QStringLiteral("MRC PDF")), "Degraded clause must survive");
        QVERIFY2(filter.contains(QStringLiteral("limited")),
                 "a Degraded clause must be visibly marked as limited");
    }

    // ── 5. Engine probes (registerEngineProbes) tell the truth ───────────────

    void engineProbesReportOfficeImportTruthfully() {
        CapabilityRegistry reg;
        reg.registerEngineProbes();

        const Capability c = reg.query(CapId::OfficeImport);
        QCOMPARE(c.status == Availability::Available,
                 ConversionManager::isOfficeImportAvailable());
        if (c.status != Availability::Available) {
            QVERIFY2(!c.whyNot.trimmed().isEmpty(), "missing converter must explain why");
            QVERIFY2(!c.alternative.trimmed().isEmpty(), "missing converter must offer the install path");
            QVERIFY2(c.alternative.contains(QStringLiteral("LibreOffice"), Qt::CaseInsensitive),
                     "the alternative must name LibreOffice (existing tested phrasing)");
        }
    }

    void engineProbesR12PassesAreBuildUnavailableWithCanonicalWording() {
        CapabilityRegistry reg;
        reg.registerEngineProbes();

        // §9.13 (21a387c): the unused-object sweep is implemented, so font
        // subsetting is the only remaining unimplemented R12 pass.
        const Capability subset = reg.query(CapId::CompressSubsetFonts);
        QCOMPARE(subset.status, Availability::UnavailableBuild);
        QCOMPARE(subset.whyNot, gp::r12UnsupportedPassExplanation());
        QVERIFY2(!subset.alternative.trimmed().isEmpty(),
                 "the R12 pass must point at the passes that DO run");

        const Capability removeUnused = reg.query(CapId::CompressRemoveUnused);
        QCOMPARE(removeUnused.status, Availability::Available);
        QVERIFY2(removeUnused.whyNot.trimmed().isEmpty(),
                 "an available pass must not carry a whyNot");
        QVERIFY2(removeUnused.detail.contains(QStringLiteral("sweep"), Qt::CaseInsensitive),
                 "the disclosure must state what actually runs");
    }

    void engineProbesWordExcelExportAlwaysAvailableWithWriterDetail() {
        CapabilityRegistry reg;
        reg.registerEngineProbes();

        const Capability word = reg.query(CapId::WordExport);
        QCOMPARE(word.status, Availability::Available);
        QVERIFY2(word.detail.contains(QStringLiteral("OOXML")),
                 "the pre-dialog disclosure must state the real format written");
        QVERIFY2(word.detail.contains(QStringLiteral("WordprocessingML")),
                 qPrintable(QStringLiteral("Word disclosure must name the actual writer; got '%1'")
                                    .arg(word.detail)));

        const Capability excel = reg.query(CapId::ExcelExport);
        QCOMPARE(excel.status, Availability::Available);
        QVERIFY2(excel.detail.contains(QStringLiteral("SpreadsheetML")),
                 qPrintable(QStringLiteral("Excel disclosure must name the actual writer; got '%1'")
                                    .arg(excel.detail)));
    }

    void engineProbesOcrLanguageDataStates() {
        CapabilityRegistry reg;
        reg.registerEngineProbes();

        // A supported language is never Unavailable*: the traineddata is either
        // present (Available) or downloadable on first use (Degraded).
        const Capability en = reg.query(CapId::OcrLanguageData, QStringLiteral("EN"));
        QVERIFY2(en.status == Availability::Available || en.status == Availability::Degraded,
                 qPrintable(QStringLiteral("EN must be Available/Degraded; got status %1")
                                    .arg(int(en.status))));
        if (en.status == Availability::Degraded) {
            QVERIFY2(en.alternative.contains(QStringLiteral("download"), Qt::CaseInsensitive),
                     "a Degraded language must disclose the download path");
            QVERIFY2(!en.whyNot.trimmed().isEmpty(), "Degraded must still explain itself");
        }

        // An unsupported language code is UnavailableRuntime (never silently
        // remapped): it must explain and point at the supported list.
        const Capability zz = reg.query(CapId::OcrLanguageData, QStringLiteral("ZZ"));
        QCOMPARE(zz.status, Availability::UnavailableRuntime);
        QVERIFY(!zz.whyNot.trimmed().isEmpty());
        QVERIFY(!zz.alternative.trimmed().isEmpty());

        // EN and ZZ cache separately (both queryable without cross-talk).
        QCOMPARE(reg.query(CapId::OcrLanguageData, QStringLiteral("EN")).status, en.status);
        QCOMPARE(reg.query(CapId::OcrLanguageData, QStringLiteral("ZZ")).status,
                 Availability::UnavailableRuntime);
    }

    void engineProbesRapidModelsFollowCompileFloor() {
        CapabilityRegistry reg;
        reg.registerEngineProbes();

        const Capability c = reg.query(CapId::OcrRapidModels);
#ifdef HAS_RAPIDOCR
        QVERIFY2(c.status == Availability::Available
                 || c.status == Availability::UnavailableRuntime,
                 "with the engine compiled in, only the runtime probe decides");
#else
        QCOMPARE(c.status, Availability::UnavailableBuild);
#endif
        if (c.status != Availability::Available) {
            QVERIFY2(!c.whyNot.trimmed().isEmpty(),
                     "missing PP-OCRv5 models must explain why (EditController wording)");
            QVERIFY2(!c.alternative.trimmed().isEmpty(),
                     "missing PP-OCRv5 models must offer the Preferences/install alternative");
        }
    }

    // ── D06: probeRapidModelsIn demands a REAL model set ────────────────────
    void rapidModelsProbeRejectsFilenameOnly() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Zero-byte detector stub — the exact D06 repro (filename present).
        QFile det(dir.filePath(QStringLiteral("PP-OCRv5_mobile_det_infer.onnx")));
        QVERIFY(det.open(QIODevice::WriteOnly));
        det.close();
        auto c = gp::CapabilityRegistry::probeRapidModelsIn(dir.path());
        QCOMPARE(c.status, gp::Availability::UnavailableRuntime);
        QVERIFY2(c.whyNot.contains(QStringLiteral("recognizer")),
                 qPrintable(QStringLiteral("must name the missing recognizer: %1").arg(c.whyNot)));
    }

    // ── G11 (QUALITY-GATE-2026-09-09): presence is NOT readiness. The old
    // test wrote three arbitrary non-empty files and expected Available — the
    // exact reviewer repro: real init dies with "Protobuf parsing failed".
    // After the fix the probe resolves the set the way the ENGINE does
    // (RapidOcrEngine::verifyModelsIn builds the real ONNX sessions), so junk
    // payloads report UnavailableRuntime with the engine's own reason.
    void rapidModelsProbeRejectsNonOnnxPayloads() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const char* mandatory[] = {
            "PP-OCRv5_mobile_det_infer.onnx",
            "PP-OCRv5_mobile_rec_infer.onnx",
            "ppocrv5_rec_dict.txt",
        };
        for (const char* name : mandatory) {
            QFile f(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("payload");
            f.close();
        }
        auto c = gp::CapabilityRegistry::probeRapidModelsIn(dir.path());
#ifdef HAS_RAPIDOCR
        QCOMPARE(c.status, gp::Availability::UnavailableRuntime);
        QVERIFY2(c.whyNot.contains(QStringLiteral("cannot load")),
                 qPrintable(QStringLiteral("junk models must be refused with the engine's reason: %1").arg(c.whyNot)));
        QVERIFY2(!c.detail.trimmed().isEmpty(),
                 "the refusal must disclose the searched directory");
#else
        // Without the engine compiled in the probe cannot verify anything —
        // it must NOT report Available on mere file presence either.
        QCOMPARE(c.status, gp::Availability::UnavailableRuntime);
#endif
    }

    // G11: a model set that the ENGINE actually loads is Available; the
    // textline classifier stays optional and is disclosed as absent. Requires
    // real PP-OCRv5 models, which this environment does not bundle — the
    // same inherent skip as RapidOCR inference (models ship separately).
    void rapidModelsProbeRealSetIsAvailableWithoutClassifier() {
        const QString modelsDir = GLYPH_OCR_MODELS_DIR;
        const QFileInfo det(modelsDir + QStringLiteral("/PP-OCRv5_mobile_det_infer.onnx"));
        const QFileInfo rec(modelsDir + QStringLiteral("/PP-OCRv5_mobile_rec_infer.onnx"));
        if (!det.isFile() || !rec.isFile())
            QSKIP("Real PP-OCRv5 ONNX models are not present in this environment (inherent skip).");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const char* name : { "PP-OCRv5_mobile_det_infer.onnx",
                                  "PP-OCRv5_mobile_rec_infer.onnx",
                                  "ppocrv5_rec_dict.txt" }) {
            QFile src(modelsDir + QLatin1Char('/') + QString::fromLatin1(name));
            QVERIFY(src.copy(dir.filePath(QString::fromLatin1(name))));
        }
        auto c = gp::CapabilityRegistry::probeRapidModelsIn(dir.path());
        QCOMPARE(c.status, gp::Availability::Available);   // classifier is OPTIONAL
        QVERIFY2(c.detail.contains(QStringLiteral("classifier")),
                 qPrintable(QStringLiteral("optional-classifier absence must be disclosed: %1").arg(c.detail)));
    }

    void rapidModelsProbeEmptyOrUnreadableFilesAreRejected() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const char* name : { "PP-OCRv5_mobile_det_infer.onnx",
                                  "PP-OCRv5_mobile_rec_infer.onnx" }) {
            QFile f(dir.filePath(QString::fromLatin1(name))); // zero bytes
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.close();
        }
        QFile dict(dir.filePath(QStringLiteral("ppocrv5_rec_dict.txt")));
        QVERIFY(dict.open(QIODevice::WriteOnly));
        dict.write("vocab");
        dict.close();
        auto c = gp::CapabilityRegistry::probeRapidModelsIn(dir.path());
        QCOMPARE(c.status, gp::Availability::UnavailableRuntime);
        QVERIFY2(c.whyNot.contains(QStringLiteral("recognizer")),
                 qPrintable(QStringLiteral("empty recognizer is not usable: %1").arg(c.whyNot)));
    }
};

QTEST_MAIN(TestCapabilityRegistry)
#include "TestCapabilityRegistry.moc"
