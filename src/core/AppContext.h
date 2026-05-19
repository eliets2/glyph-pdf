#pragma once

#include <memory>

class IPdfEditorEngine;
class IOcrEngine;
class IFormManager;
class ISignatureManager;
class IConversionEngine;
class ICollaboration;
class QUndoStack;
class DocumentSession;

struct AppContext {
    std::shared_ptr<IPdfEditorEngine>   pdfEditor;
    std::shared_ptr<IOcrEngine>         ocr;
    std::shared_ptr<IFormManager>       forms;
    std::shared_ptr<ISignatureManager>  signing;
    std::shared_ptr<IConversionEngine>  conversion;
    std::shared_ptr<ICollaboration>     collab;
    std::shared_ptr<QUndoStack>         undoStack;
    std::shared_ptr<DocumentSession>    document;

    static constexpr const char* DefaultCloudSyncEndpoint = "https://weaver.enterprise.internal/v1/sync";
};
