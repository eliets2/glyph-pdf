#include "Bootstrapper.h"
#include "core/AppContext.h"

#include "engines/OcrEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/FormManager.h"
#include "engines/SignatureManager.h"
#include "engines/ConversionManager.h"
#include "engines/CollaborationManager.h"
#include "engines/DocumentSession.h"
#include "engines/AutosaveManager.h"

#include <QUndoStack>
#include <memory>

AppContext Bootstrapper::createContext() {
    AppContext ctx;

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
