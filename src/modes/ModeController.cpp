#include "ModeController.h"

#include "ui/PdfViewerWidget.h"
#include "modes/OCRMode.h"
#include "modes/RedactMode.h"
#include "modes/CompareMode.h"
#include "modes/PagesMode.h"
#include "modes/BatchMode.h"
#include "modes/FormBuilderMode.h"

namespace gp {

ModeController::ModeController(QWidget* parent) : QStackedWidget(parent) {
    _viewer = new PdfViewerWidget(this);
    _byId.insert("",      _viewer);
    _byId.insert("ocr",       nullptr);
    _byId.insert("redact",    nullptr);
    _byId.insert("compare",   nullptr);
    _byId.insert("pages",     nullptr);
    _byId.insert("batch",     nullptr);
    _byId.insert("form",      nullptr);

    addWidget(_viewer);
    setCurrentWidget(_viewer);
}

void ModeController::setScreen(const QString& id) {
    if (_currentScreen == id) return;
    _currentScreen = id;
    // Screens that are panel-only (no center swap): signature, ai, pdfa, compress, watermark
    if (id == "signature" || id == "ai" || id == "pdfa" ||
        id == "compress"  || id == "watermark") {
        setCurrentWidget(_viewer);
        emit screenChanged(id);
        return;
    }

    // Lazy initialization
    if (_byId.contains(id) && _byId[id] == nullptr) {
        QWidget* target = nullptr;
        if (id == "ocr")          target = new OCRMode(this);
        else if (id == "redact")  target = new RedactMode(this);
        else if (id == "compare") target = new CompareMode(this);
        else if (id == "pages")   target = new PagesMode(this);
        else if (id == "batch")   target = new BatchMode(this);
        else if (id == "form")    target = new FormBuilderMode(this);

        if (target) {
            _byId[id] = target;
            addWidget(target);
        }
    }

    auto* target = _byId.value(id, _viewer);
    if (!target) target = _viewer;
    setCurrentWidget(target);
    emit screenChanged(id);
}

} // namespace gp

