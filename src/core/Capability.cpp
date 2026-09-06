// SPDX-License-Identifier: Apache-2.0
#include "core/Capability.h"

#include "core/OcrTypes.h"               // ocrLanguages() — the language table
#include "engines/ConversionManager.h"   // locateSoffice / isOfficeImportAvailable
#include "engines/VeraPdfValidator.h"    // isAvailable / locateCli (runtime)

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QWidget>

#include <utility>

namespace gp {

// ── Canonical user-facing wording ────────────────────────────────────────────
// Moved here so the registry is the single source of truth; the original call
// sites (CompressDialog::unsupportedPassExplanation, HomeController's import
// dialog, CompressDialog's MRC fallback) delegate to / match these strings so
// the existing test anchors keep their exact user-visible wording.

// R12 — byte-identical to the wording TestCompressDialogHonesty pins via
// CompressDialog::unsupportedPassExplanation() (which now delegates here).
QString r12UnsupportedPassExplanation()
{
    return QObject::tr("Not available in this build: the compression engine does not "
                       "implement font subsetting or unused-object removal, so these "
                       "passes would not run.");
}

// LibreOffice import — verbatim from the HomeController dialog
// (HomeController::onImportOffice), now owned by the registry.
QString officeImportWhyNot()
{
    return QObject::tr("Importing Word, Excel and PowerPoint files to PDF uses "
                       "LibreOffice, which doesn't appear to be installed.");
}

QString officeImportAlternative()
{
    return QObject::tr("LibreOffice is free and open source. Install it once, "
                       "then this feature works automatically — no GlyphPDF "
                       "restart required.");
}

// MRC fallback — from the CompressDialog pre-compression disclosure, promoted
// from a mid-run info dialog to a before-execution explanation.
QString mrcWhyNot()
{
    return QObject::tr("MRC (Mixed Raster Content) compression requires the document "
                       "to have been processed through the OCR pipeline first, so "
                       "pre-rendered page images exist.");
}

QString mrcAlternative()
{
    return QObject::tr("Run OCR on the document first, then use Export \u2192 MRC PDF/A "
                       "from the File menu — or use standard compression here.");
}

// U08 signature-kind disclosure: the Draw/Type/Upload picker produces a
// GRAPHIC stamp — a different thing from the certificate-backed P12 digital
// signature flow. Surfaced in the SignaturePickerDialog so the kind is named
// before the user commits.
QString visibleSignatureKindDisclosure()
{
    return QObject::tr("Graphic signature: Draw, Type and Upload place a signature "
                       "stamp (an image annotation) on the page. This is not a "
                       "certificate-backed digital signature — use the P12 signing "
                       "flow for a cryptographically verifiable signature.");
}

// ── Registry core ─────────────────────────────────────────────────────────────

CapabilityRegistry::CapabilityRegistry(QObject* parent)
    : QObject(parent)
{
}

void CapabilityRegistry::registerProbe(CapId id, Probe probe)
{
    m_probes.insert(int(id), std::move(probe));
    m_cache.remove(cacheKey(id, QVariant()));
}

QString CapabilityRegistry::cacheKey(CapId id, const QVariant& param)
{
    return QString::number(int(id)) + QLatin1Char('\n') + param.toString();
}

Capability CapabilityRegistry::query(CapId id, const QVariant& param) const
{
    const QString key = cacheKey(id, param);
    const auto hit = m_cache.constFind(key);
    if (hit != m_cache.constEnd())
        return hit.value();

    Capability result;
    const auto probe = m_probes.constFind(int(id));
    if (probe == m_probes.constEnd()) {
        // Unregistered capability: structurally unknown to this registry.
        // Never fake availability; explain and point at a real alternative.
        result.status = Availability::UnavailableBuild;
        result.whyNot = QObject::tr("This capability is not registered in this build.");
        result.alternative = QObject::tr("Choose one of the supported formats or operations.");
        qWarning("CapabilityRegistry: no probe registered for CapId(%d)", int(id));
    } else {
        result = (*probe)(param);
        // Non-empty rule: every Unavailable*/Degraded result must explain
        // itself and offer a supported alternative. Lazy providers get a
        // generic fallback plus a qWarning so tests catch them in the log.
        if (result.status != Availability::Available
            && (result.whyNot.trimmed().isEmpty() || result.alternative.trimmed().isEmpty())) {
            qWarning("CapabilityRegistry: CapId(%d) reported status %d with empty "
                     "whyNot/alternative — lazy provider",
                     int(id), int(result.status));
            if (result.whyNot.trimmed().isEmpty())
                result.whyNot = QObject::tr("This capability is not available right now.");
            if (result.alternative.trimmed().isEmpty())
                result.alternative = QObject::tr("Choose one of the supported formats or operations.");
        }
    }

    m_cache.insert(key, result);
    return result;
}

bool CapabilityRegistry::available(CapId id, const QVariant& param) const
{
    return query(id, param).status == Availability::Available;
}

void CapabilityRegistry::invalidate(CapId id)
{
    // Drop every parameterized entry of this id.
    const QString prefix = QString::number(int(id)) + QLatin1Char('\n');
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (it.key().startsWith(prefix))
            it = m_cache.erase(it);
        else
            ++it;
    }
    emit capabilityChanged(id);
}

void CapabilityRegistry::invalidateAll()
{
    m_cache.clear();
    for (auto it = m_probes.constBegin(); it != m_probes.constEnd(); ++it)
        emit capabilityChanged(CapId(it.key()));
}

QString CapabilityRegistry::combineWhyNot(const Capability& c)
{
    if (c.status == Availability::Available)
        return {};
    QString text = c.whyNot.trimmed();
    const QString alt = c.alternative.trimmed();
    if (!alt.isEmpty()) {
        if (!text.isEmpty())
            text += QLatin1Char(' ');
        text += alt;
    }
    if (text.isEmpty())
        text = QObject::tr("This capability is not available right now.");
    return text;
}

void CapabilityRegistry::applyToWidget(QWidget* w, CapId id, const QVariant& param) const
{
    if (!w)
        return;
    const Capability c = query(id, param);
    switch (c.status) {
    case Availability::Available:
        return;                                  // no-op — the control stays as-is
    case Availability::Degraded:
        // Disclose but keep the control usable.
        w->setToolTip(c.detail.isEmpty() ? combineWhyNot(c) : c.detail);
        return;
    case Availability::UnavailableBuild:
    case Availability::UnavailableRuntime:
        w->setEnabled(false);
        w->setToolTip(combineWhyNot(c));
        w->setStatusTip(combineWhyNot(c));
        return;
    }
}

QString CapabilityRegistry::fileFilterFor(const QList<QPair<QString, CapId>>& clauses,
                                          const QString& allLabel) const
{
    QStringList kept;
    for (const auto& clause : clauses) {
        const Capability c = query(clause.second);
        if (c.status == Availability::UnavailableBuild
            || c.status == Availability::UnavailableRuntime)
            continue;                            // never offer what cannot be produced
        QString text = clause.first;
        if (c.status == Availability::Degraded) {
            // Visibly mark the limited clause without breaking "Label (*.ext)".
            const int cut = text.indexOf(QLatin1String(" ("));
            text = (cut >= 0)
                ? text.left(cut) + QStringLiteral(" (limited)") + text.mid(cut)
                : text + QStringLiteral(" (limited)");
        }
        kept << text;
    }
    if (kept.isEmpty())
        return allLabel;
    return (allLabel.isEmpty() ? QString() : allLabel + QStringLiteral(";;")) + kept.join(QLatin1String(";;"));
}

// ── Engine probes ─────────────────────────────────────────────────────────────
// One implementation per capability. Cross-references mark where a scattered
// probe used to live (the consolidation map for the U08 migration).

namespace {

// Formerly: HomeController::onImportOffice's inline probe + dialog strings.
Capability probeOfficeImport(const QVariant&)
{
    Capability c;
    const QString soffice = ConversionManager::locateSoffice();
    if (!soffice.isEmpty()) {
        c.status = Availability::Available;
        c.detail = QObject::tr("LibreOffice converter found: %1").arg(soffice);
        return c;
    }
    c.status = Availability::UnavailableRuntime;
    c.whyNot = officeImportWhyNot();
    c.alternative = officeImportAlternative();
    return c;
}

// Real Word/Excel export exists in EVERY build (R10): the vendored OOXML lib
// when compiled in, GlyphPDF's in-house writers otherwise. The disclosure
// names the writer that WILL run — the §9.16 honest badge, moved before the
// file dialog. Formerly: the post-write disclosure in ConvertController.
Capability probeWordExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
#ifdef HAS_DUCKX
    c.detail = QObject::tr("Real Word OOXML: written with the vendored duckx "
                           "WordprocessingML library.");
#else
    c.detail = QObject::tr("Real Word OOXML: written with GlyphPDF's built-in "
                           "WordprocessingML writer.");
#endif
    return c;
}

Capability probeExcelExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
#ifdef HAS_OPENXLSX
    c.detail = QObject::tr("Real Excel OOXML: written with the vendored OpenXLSX "
                           "SpreadsheetML library.");
#else
    c.detail = QObject::tr("Real Excel OOXML: written with GlyphPDF's built-in "
                           "SpreadsheetML writer.");
#endif
    return c;
}

Capability probePptExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("Real PowerPoint OOXML: written with GlyphPDF's built-in "
                           "PPTX writer.");
    return c;
}

Capability probeCsvExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("UTF-8 CSV of the document's extracted text rows.");
    return c;
}

Capability probeHtmlExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("Self-contained HTML of the document's extracted content.");
    return c;
}

Capability probeTextExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("Plain UTF-8 text of the document's extracted content.");
    return c;
}

Capability probeImageExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("Pages rendered on-device and saved as PNG/JPEG/TIFF.");
    return c;
}

Capability probePdfAExport(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("PDF/A-1b/2b/3b conversion via the built-in engines.");
    return c;
}

Capability probeLinearize(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("Fast Web View linearization via the built-in engines.");
    return c;
}

// Formerly: EditController::runOcr's inline ppocrv5 path probe
// (EditController.cpp:589-601) — the single shared implementation now.
Capability probeOcrRapidModels(const QVariant&)
{
    Capability c;
#ifdef HAS_RAPIDOCR
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                            + QStringLiteral("/models/ppocrv5");
    const QString nextToExe = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/models/ppocrv5");
    const QString detModel = QStringLiteral("/PP-OCRv5_mobile_det_infer.onnx");
    if (QFile::exists(appData + detModel)) {
        c.status = Availability::Available;
        c.detail = appData + detModel;
        return c;
    }
    if (QFile::exists(nextToExe + detModel)) {
        c.status = Availability::Available;
        c.detail = nextToExe + detModel;
        return c;
    }
    c.status = Availability::UnavailableRuntime;
    c.whyNot = QObject::tr("PP-OCRv5 ONNX models not found.");
    c.alternative = QObject::tr("Change the OCR engine in Preferences \u2192 Engines, "
                                "or install the models.");
    c.detail = QObject::tr("Searched: %1 and %2").arg(appData + detModel, nextToExe + detModel);
#else
    // Compile-time floor only: the engine is structurally absent from this
    // binary, so UnavailableBuild is the honest state.
    c.status = Availability::UnavailableBuild;
    c.whyNot = QObject::tr("This build was compiled without the RapidOCR (ONNX) engine.");
    c.alternative = QObject::tr("Use the Tesseract 5 engine, or install a build with "
                                "RapidOCR enabled.");
#endif
    return c;
}

Capability probeOcrTesseract(const QVariant&)
{
    Capability c;
#ifdef HAS_TESSERACT
    c.status = Availability::Available;
    c.detail = QObject::tr("Tesseract 5 LSTM engine (compiled into this build).");
#else
    c.status = Availability::UnavailableBuild;
    c.whyNot = QObject::tr("OCR is not compiled into this build.");
    c.alternative = QObject::tr("Install a build with Tesseract enabled.");
#endif
    return c;
}

Capability probeOcrEnsemble(const QVariant&)
{
    Capability c;
#ifdef HAS_RAPIDOCR
    if (probeOcrRapidModels({}).status == Availability::Available) {
        c.status = Availability::Available;
        c.detail = QObject::tr("ROVER ensemble of Tesseract 5 + RapidOCR/PP-OCRv5.");
        return c;
    }
    // Auto-degrade is the documented interactive behavior (EditController
    // "auto" resolution): the ensemble runs Tesseract alone. Degraded — not
    // unavailable — because OCR still works.
    c.status = Availability::Degraded;
    c.whyNot = QObject::tr("The PP-OCRv5 models are not installed, so the ensemble "
                           "runs Tesseract only.");
    c.alternative = QObject::tr("Install the PP-OCRv5 models, or keep the Tesseract engine.");
#else
    c.status = Availability::UnavailableBuild;
    c.whyNot = QObject::tr("This build was compiled without the RapidOCR (ONNX) engine.");
    c.alternative = QObject::tr("Use the Tesseract 5 engine, or install a build with "
                                "RapidOCR enabled.");
#endif
    return c;
}

// Per-language traineddata. Mirrors OcrEngine::initialize's data path logic
// (OcrEngine.cpp:196-224: AppLocalData/tessdata seeded from the bundled
// exe/tessdata copy, then tessdata_best download on first use): a supported
// language is therefore Available (local) or Degraded (downloadable) — never
// blocked outright. Formerly disclosed nowhere before the run.
Capability probeOcrLanguageData(const QVariant& param)
{
    Capability c;
    const QString uiCode = param.toString().trimmed();

    QString engineCode;
    QString displayName;
    bool known = false;
    for (const auto& l : ocrLanguages()) {
        if (uiCode.compare(QLatin1String(l.uiCode), Qt::CaseInsensitive) == 0) {
            known = true;
            engineCode = QLatin1String(l.engineCode);
            displayName = QString::fromUtf8(l.displayName);
            break;
        }
    }
    if (!known) {
        // Never silently remap an unknown language to "eng" at the disclosure
        // layer — the user must see that their choice is unsupported.
        c.status = Availability::UnavailableRuntime;
        c.whyNot = QObject::tr("\u201C%1\u201D is not a supported OCR language.").arg(uiCode);
        c.alternative = QObject::tr("Choose one of the languages offered in the list.");
        return c;
    }

#ifndef HAS_TESSERACT
    Q_UNUSED(displayName);
    c.status = Availability::UnavailableBuild;
    c.whyNot = QObject::tr("OCR is not compiled into this build.");
    c.alternative = QObject::tr("Install a build with Tesseract enabled.");
    return c;
#else
    const QString filename = engineCode + QStringLiteral(".traineddata");
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                            + QStringLiteral("/tessdata/") + filename;
    if (QFile::exists(appData)) {
        c.status = Availability::Available;
        c.detail = appData;
        return c;
    }
    const QString bundled = QCoreApplication::applicationDirPath()
                            + QStringLiteral("/tessdata/") + filename;
    if (QFile::exists(bundled)) {
        c.status = Availability::Available;
        c.detail = bundled;
        return c;
    }
    c.status = Availability::Degraded;
    c.whyNot = QObject::tr("The %1 language data is not installed yet.").arg(displayName);
    c.alternative = QObject::tr("It will be downloaded from the tesseract-ocr tessdata_best "
                                "project on first use — this requires network access once.");
    c.detail = QObject::tr("Searched: %1 and %2").arg(appData, bundled);
    return c;
#endif
}

// R12: the compression backend implements neither font subsetting nor
// unused-object removal. Compile-time truth → UnavailableBuild. Formerly:
// CompressDialog::unsupportedPassExplanation's static label (now delegates to
// the canonical string).
Capability probeCompressSubsetFonts(const QVariant&)
{
    Capability c;
    c.status = Availability::UnavailableBuild;
    c.whyNot = r12UnsupportedPassExplanation();
    c.alternative = QObject::tr("Use image downsampling and deduplication instead — "
                                "those passes run in this build.");
    c.detail = QObject::tr("R12: no font subsetter is implemented in the compression backend.");
    return c;
}

Capability probeCompressRemoveUnused(const QVariant&)
{
    Capability c;
    c.status = Availability::UnavailableBuild;
    c.whyNot = r12UnsupportedPassExplanation();
    c.alternative = QObject::tr("Use image downsampling and deduplication instead — "
                                "those passes run in this build.");
    c.detail = QObject::tr("R12: no object garbage collector is implemented in the "
                           "compression backend.");
    return c;
}

// MRC needs pre-rendered page images from the OCR pipeline, which the
// compress dialog does not have — a degraded, not absent, capability.
// Formerly: a mid-run info dialog + silent fallback (CompressDialog.cpp).
Capability probeMrcCompression(const QVariant&)
{
    Capability c;
    c.status = Availability::Degraded;
    c.whyNot = mrcWhyNot();
    c.alternative = mrcAlternative();
    c.detail = QObject::tr("JBIG2 foreground + JPEG2000 background; requires "
                           "pre-rendered page images from the OCR pipeline.");
    return c;
}

Capability probeDigitalSignature(const QVariant&)
{
    Capability c;
    // OpenSSL is linked unconditionally (pdfws_engines); the PKCS#7 signing
    // path (PdfEncryptPubSec) is compiled in every build.
    c.status = Availability::Available;
    c.detail = QObject::tr("Certificate-backed digital signature (X.509/P12, PKCS#7 "
                           "via OpenSSL) — cryptographically verifiable, distinct "
                           "from a graphic signature stamp.");
    return c;
}

Capability probeVisibleSignatureGraphic(const QVariant&)
{
    Capability c;
    c.status = Availability::Available;
    c.detail = QObject::tr("Draw, Type and Upload place a graphic signature stamp — "
                           "an image annotation, not a certificate-backed digital "
                           "signature.");
    return c;
}

Capability probePdfAValidation(const QVariant&)
{
    Capability c;
    if (VeraPdfValidator::isAvailable()) {
        c.status = Availability::Available;
        c.detail = VeraPdfValidator::locateCli();
        return c;
    }
    c.status = Availability::UnavailableRuntime;
    c.whyNot = QObject::tr("The veraPDF validator was not found on this machine.");
    c.alternative = QObject::tr("Install veraPDF (bundled copy next to the executable, "
                                "or on the PATH) to validate PDF/A conformance.");
    return c;
}

} // namespace

void CapabilityRegistry::registerEngineProbes()
{
    registerProbe(CapId::OfficeImport,          probeOfficeImport);
    registerProbe(CapId::WordExport,            probeWordExport);
    registerProbe(CapId::ExcelExport,           probeExcelExport);
    registerProbe(CapId::PptExport,             probePptExport);
    registerProbe(CapId::CsvExport,             probeCsvExport);
    registerProbe(CapId::HtmlExport,            probeHtmlExport);
    registerProbe(CapId::TextExport,            probeTextExport);
    registerProbe(CapId::ImageExport,           probeImageExport);
    registerProbe(CapId::PdfAExport,            probePdfAExport);
    registerProbe(CapId::Linearize,             probeLinearize);
    registerProbe(CapId::OcrTesseract,          probeOcrTesseract);
    registerProbe(CapId::OcrRapidModels,        probeOcrRapidModels);
    registerProbe(CapId::OcrEnsemble,           probeOcrEnsemble);
    registerProbe(CapId::OcrLanguageData,       probeOcrLanguageData);
    registerProbe(CapId::CompressSubsetFonts,   probeCompressSubsetFonts);
    registerProbe(CapId::CompressRemoveUnused,  probeCompressRemoveUnused);
    registerProbe(CapId::MrcCompression,        probeMrcCompression);
    registerProbe(CapId::DigitalSignature,      probeDigitalSignature);
    registerProbe(CapId::VisibleSignatureGraphic, probeVisibleSignatureGraphic);
    registerProbe(CapId::PdfAValidation,        probePdfAValidation);
}

} // namespace gp
