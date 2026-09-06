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

        for (const CapId id : { CapId::CompressSubsetFonts, CapId::CompressRemoveUnused }) {
            const Capability c = reg.query(id);
            QCOMPARE(c.status, Availability::UnavailableBuild);
            QCOMPARE(c.whyNot, gp::r12UnsupportedPassExplanation());
            QVERIFY2(!c.alternative.trimmed().isEmpty(),
                     "the R12 passes must point at the passes that DO run");
        }
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
};

QTEST_MAIN(TestCapabilityRegistry)
#include "TestCapabilityRegistry.moc"
