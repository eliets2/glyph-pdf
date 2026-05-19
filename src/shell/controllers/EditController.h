#pragma once

#include <QObject>
#include <QString>
#include <QRectF>

struct AppContext;

namespace gp {

class MainWindow;

class EditController : public QObject {
    Q_OBJECT
public:
    EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    bool handles(const QString& toolId) const;
    void activate(const QString& toolId);

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
