#pragma once
#include <QStackedWidget>
#include <QHash>

class PdfViewerWidget;

namespace gp {

class OCRMode;
class RedactMode;
class CompareMode;
class PagesMode;
class BatchMode;
class FormBuilderMode;

// Owns the central area: one QStackedWidget routing between Standard canvas and
// the 11 extended mode widgets.
class ModeController : public QStackedWidget {
    Q_OBJECT
public:
    explicit ModeController(QWidget* parent = nullptr);

    void setScreen(const QString& id);     // "" / "ocr" / "redact" / ...
    QString currentScreen() const { return _currentScreen; }

    PdfViewerWidget* viewer() const { return _viewer; }

signals:
    void screenChanged(const QString& id);

private:
    QHash<QString, QWidget*> _byId;
    QString     _currentScreen;
    PdfViewerWidget* _viewer = nullptr;
};

} // namespace gp

