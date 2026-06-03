// SPDX-License-Identifier: Apache-2.0
#include "Bootstrapper.h"
#include "core/AppContext.h"

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

    // Djot foundation (M4-PROMPT-7)
    std::string djotPath = (QCoreApplication::applicationDirPath() + "/../third_party/djot").toStdString();
    ctx.djotCodec       = std::make_shared<pdfws::LuaDjotCodec>(djotPath);
    ctx.djotMapper      = std::make_shared<pdfws::PdfStructureMapper>();
    ctx.provenanceGuard = std::make_shared<pdfws::ProvenanceGuard>();

    return ctx;
}

