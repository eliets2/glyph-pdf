#pragma once

#include <memory>

// Forward declarations — no heavy engine headers in AppContext.h
// (AppContext.h is included by pdfws_ui which does not link Qt6::Concurrent)
class IPdfEditorEngine;
class IOcrEngine;
class IFormManager;
class ISignatureManager;
class IConversionEngine;
class ICollaboration;
class QUndoStack;
class DocumentSession;
class AutosaveManager;

namespace gp { class LaneScheduler; }

struct AppContext {
    std::shared_ptr<gp::LaneScheduler>  scheduler;
    std::shared_ptr<IPdfEditorEngine>   pdfEditor;
    std::shared_ptr<IOcrEngine>         ocr;
    std::shared_ptr<IFormManager>       forms;
    std::shared_ptr<ISignatureManager>  signing;
    std::shared_ptr<IConversionEngine>  conversion;
    std::shared_ptr<ICollaboration>     collab;
    std::shared_ptr<QUndoStack>         undoStack;
    std::shared_ptr<DocumentSession>    document;
    std::shared_ptr<AutosaveManager>    autosave;

    static constexpr const char* DefaultCloudSyncEndpoint = "https://weaver.enterprise.internal/v1/sync";
};
