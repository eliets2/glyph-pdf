#pragma once

#include <QObject>
#include <QString>
#include <QRectF>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

struct AppContext;
class EditToolBar;

namespace gp {

class MainWindow;

class EditController : public QObject, public IToolController {
    Q_OBJECT
public:
    EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    // Search / replace slots wired from FindBar
    void onSearchRequested(const QString &text, bool forward, bool matchCase,
                           bool wholeWords, bool useRegex, int scope);
    void onReplaceRequested(const QString &searchText, const QString &replaceText,
                            bool matchCase, bool wholeWords, bool useRegex);
    void onReplaceAllRequested(const QString &searchText, const QString &replaceText,
                               bool matchCase, bool wholeWords, bool useRegex);
    void onRedactAllRequested(const QString &text, bool matchCase, bool wholeWords);

private slots:
    void onImageSelected(const QString &name, const QRectF &placement);
    void onImageMoved(const QString &name, double dx, double dy);
    void onImageResized(const QString &name, double newW, double newH);
    void onTextEditRequested(int pageIndex, QPointF pos);
    void onTextFormatChanged(const QString &fontFamily, int fontSize, const QColor &color, bool bold, bool italic, int alignment);

private:
    void runOcr();
    void editPdfText();
    void enterImageEditMode();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    bool _ocrRunning = false;
    QString _selectedImageName;
    int _imageEditPage = -1;
    EditToolBar* _textToolBar = nullptr;

    // Text formatting state
    QString _fontFamily = "Helvetica";
    int _fontSize = 12;
    QColor _fontColor = Qt::black;
    bool _fontBold = false;
    bool _fontItalic = false;
    int _fontAlignment = 0;

    // Search state for match navigation
    int _currentMatchIndex = -1;
    int _totalMatches = 0;
};

} // namespace gp
