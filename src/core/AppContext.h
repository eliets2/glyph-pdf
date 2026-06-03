// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>

// Forward declarations — no heavy engine headers in AppContext.h
// (AppContext.h is included by pdfws_ui which does not link Qt6::Concurrent)
class IPdfEditorEngine;
class IOcrEngine;
class IFormManager;
class ISignatureManager;
class IConversionEngine;
class QUndoStack;
class DocumentSession;
class AutosaveManager;

namespace gp { class LaneScheduler; }

namespace pdfws {
    class IDjotCodec;
    class IDjotMapper;
    class ProvenanceGuard;
}

struct AppContext {
    std::shared_ptr<gp::LaneScheduler>  scheduler;
    std::shared_ptr<IPdfEditorEngine>   pdfEditor;
    std::shared_ptr<IOcrEngine>         ocr;
    std::shared_ptr<IFormManager>       forms;
    std::shared_ptr<ISignatureManager>  signing;
    std::shared_ptr<IConversionEngine>  conversion;
    std::shared_ptr<QUndoStack>         undoStack;
    std::shared_ptr<DocumentSession>    document;
    std::shared_ptr<AutosaveManager>    autosave;

    // Djot foundation
    std::shared_ptr<pdfws::IDjotCodec>      djotCodec;
    std::shared_ptr<pdfws::IDjotMapper>     djotMapper;
    std::shared_ptr<pdfws::ProvenanceGuard> provenanceGuard;
};
