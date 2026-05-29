#include "Bootstrapper.h"
#include "core/AppContext.h"

#include "engines/scheduling/LaneScheduler.h"
#include "engines/OcrEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/FormManager.h"
#include "engines/SignatureManager.h"
#include "engines/ConversionManager.h"
#include "engines/CollaborationManager.h"
#include "engines/DocumentSession.h"
#include "engines/AutosaveManager.h"

#include <QThread>
#include <QUndoStack>
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
    ctx.collab     = std::make_shared<CollaborationManager>();
    ctx.undoStack  = std::make_shared<QUndoStack>();
    ctx.undoStack->setUndoLimit(200);
    ctx.document   = std::make_shared<DocumentSession>();
    ctx.autosave   = std::make_shared<AutosaveManager>(ctx.pdfEditor, ctx.document);

    return ctx;
}
