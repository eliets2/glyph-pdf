#pragma once
#include <QStatusBar>

class QLabel;
class QPdfDocument;
class IPdfEditorEngine;

namespace gp {

class StatusBar : public QStatusBar {
    Q_OBJECT
public:
    explicit StatusBar(QWidget* parent = nullptr);
    void setMode(const QString& m);
    void setTool(const QString& t);
    void setPage(int current, int total);
    void setZoom(int pct);
    void setSelection(const QString& sel);
    void setScreen(const QString& s);

    void updateDocData(QPdfDocument* doc, const QString& filePath);
    void updateUnsaved(bool dirty);
    void updateFromDocument(IPdfEditorEngine* engine, const QString& filePath);

private:
    QLabel* makeCell(const QString& text);
    QLabel* _mode = nullptr;
    QLabel* _screen = nullptr;
    QLabel* _page = nullptr;
    QLabel* _zoom = nullptr;
    QLabel* _tool = nullptr;
    QLabel* _sel  = nullptr;

    // Permanent cells
    QLabel* _ocrLang = nullptr;
    QLabel* _encoding = nullptr;
    QLabel* _pdfVersion = nullptr;
    QLabel* _pageSize = nullptr;
    QLabel* _docInfo = nullptr;
    QLabel* _unsaved = nullptr;
};

} // namespace gp
