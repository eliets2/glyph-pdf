// SPDX-License-Identifier: Apache-2.0
#include "Bootstrapper.h"
#include "core/AppContext.h"
#include "core/Capability.h"
#include "core/FormStaleFieldTracker.h"

#include "engines/scheduling/LaneScheduler.h"
#include "engines/OcrEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/FormManager.h"
#include "engines/SignatureManager.h"
#include "engines/ConversionManager.h"
#include "engines/DocumentSession.h"
#include "engines/AutosaveManager.h"

#include "pdfws_djot/ProvenanceGuard.h"
#include "pdfws_djot/LuaDjotCodec.h"
#include "pdfws_djot/PdfStructureMapper.h"

#include <QThread>
#include <QUndoStack>
#include <QCoreApplication>
#include <memory>

AppContext Bootstrapper::createContext() {
    AppContext ctx;

    // LaneScheduler is base infrastructure; constructed first so that
    // engines constructed below can obtain a reference to it if needed.
    ctx.scheduler  = std::make_shared<gp::LaneScheduler>(
        /*gpuCapacity=*/2,
        /*cpuCapacity=*/QThread::idealThreadCount());

    ctx.ocr        = std::make_shared<OcrEngine>();
    ctx.pdfEditor  = std::make_shared<PdfEditorEngine>();
    ctx.forms      = std::make_shared<FormManager>();
    ctx.signing    = std::make_shared<SignatureManager>();
    ctx.conversion = std::make_shared<ConversionManager>();
    ctx.undoStack  = std::make_shared<QUndoStack>();
    ctx.undoStack->setUndoLimit(200);
    ctx.document   = std::make_shared<DocumentSession>();
    ctx.autosave   = std::make_shared<AutosaveManager>(ctx.pdfEditor, ctx.document);

    // Djot foundation (M4-PROMPT-7, ARCH-4)
#ifndef DJOT_LIB_DIR
#  error "DJOT_LIB_DIR must be defined by CMake — check target_compile_definitions in CMakeLists.txt"
#endif
    std::string djotPath = QString::fromUtf8(DJOT_LIB_DIR).toStdString();
    
    ctx.djotCodec       = std::make_shared<pdfws::LuaDjotCodec>(djotPath);
    ctx.djotMapper      = std::make_shared<pdfws::PdfStructureMapper>();
    ctx.provenanceGuard = std::make_shared<pdfws::ProvenanceGuard>();

    // U08: the ONE capability registry, registered next to the engines whose
    // truth the probes wrap (OfficeImport→ConversionManager::locateSoffice,
    // OcrRapidModels→the ppocrv5 path probe, R12 compression passes, export
    // writers, signature kinds, veraPDF). Cached per (id, param); consumers
    // invalidate after environment changes ("Download LibreOffice…", prefs).
    ctx.capabilities = std::make_shared<gp::CapabilityRegistry>();
    ctx.capabilities->registerEngineProbes();

    // R18(a): the persistent stale-calculated-field disclosure store.
    ctx.formStale = std::make_shared<gp::FormStaleFieldTracker>();
    // N2 (backlog 2026-09-10): XFA honesty probe — param is the PDF file
    // path. The probe wraps IFormManager::hasXfaForms (the /AcroForm /XFA
    // dict check), so the open-time disclosure and any future consumer read
    // the SAME truth through the registry. XFA present = Degraded with the
    // canonical whyNot/alternative; no XFA = Available (nothing disclosed).
    std::weak_ptr<IFormManager> formsWeak = ctx.forms;
    ctx.capabilities->registerProbe(
        gp::CapId::XfaForms, [formsWeak](const QVariant& param) -> gp::Capability {
            gp::Capability c;
            c.status = gp::Availability::Available;
            const QString path = param.toString();
            auto forms = formsWeak.lock();
            if (path.isEmpty() || !forms) return c;   // nothing to disclose
            if (!forms->hasXfaForms(path)) return c;  // honest "no XFA here"
            c.status = gp::Availability::Degraded;
            c.whyNot = gp::xfaFormsWhyNot();
            c.alternative = gp::xfaFormsAlternative();
            c.detail = QStringLiteral(
                "IFormManager::hasXfaForms found an /AcroForm /XFA entry in %1")
                .arg(path);
            return c;
        });

    return ctx;
}

