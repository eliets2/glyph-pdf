#pragma once

#include <QObject>
#include <QString>
#include <QRectF>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

struct AppContext;

namespace gp {

class MainWindow;

class EditController : public QObject, public IToolController {
    Q_OBJECT
public:
    EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    void onSearchRequested(const QString &text, bool forward, bool matchCase, bool wholeWords);
    void onRedactAllRequested(const QString &text, bool matchCase, bool wholeWords);

private slots:
    void onImageSelected(const QString &name, const QRectF &placement);
    void onImageMoved(const QString &name, double dx, double dy);
    void onImageResized(const QString &name, double newW, double newH);

private:
    void runOcr();
    void editPdfText();
    void enterImageEditMode();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    bool _ocrRunning = false;
    QString _selectedImageName;
    int _imageEditPage = -1;
};

} // namespace gp
