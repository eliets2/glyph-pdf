// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QFrame>
#include "core/AnnotationTypes.h"

class QStackedWidget;
class QTabBar;
class QListWidget;
class QLabel;
class QWidget;
class ThumbnailSidebar;
class CommentsWidget;
class InspectorWidget;
class BookmarkPanel;
class PdfViewerWidget;
struct AppContext;

namespace gp {

class Sidebar : public QFrame {
    Q_OBJECT
public:
    enum Side { Left, Right };
    explicit Sidebar(Side s, QWidget* parent = nullptr);
    ~Sidebar() override;

    void init(const AppContext* ctx, PdfViewerWidget* viewer);

    // R15: pane switching is the real route behind the View-panes entries.
    // Left: "pages" | "bookmarks" | "comments" | "files".
    // Right: "properties" | "comments" | "layers". Unknown names are ignored.
    Q_INVOKABLE void showPane(const QString& name);
    // Canonical current pane name (test/debug seam for pane-switch routing).
    Q_INVOKABLE QString activePane() const;

private slots:
    void onTabChanged(int index);

private:
    void updateFilesList();
    void updateLayersList();
    void updateCommentsTab();

    Side m_side;
    const AppContext* m_ctx = nullptr;
    PdfViewerWidget* m_viewer = nullptr;
    QString m_lastFilePath;

    QTabBar* m_tabs = nullptr;
    QStackedWidget* m_stack = nullptr;

    // Left Sidebar widgets
    ThumbnailSidebar* m_thumbSidebar = nullptr;
    BookmarkPanel* m_bookmarkPanel = nullptr;
    CommentsWidget* m_commentsWidget = nullptr;
    QListWidget* m_filesList = nullptr;

    // Right Sidebar widgets
    InspectorWidget* m_inspectorWidget = nullptr;
    QListWidget* m_commentThreadList = nullptr;
    QListWidget* m_layersList = nullptr;
    QWidget* m_layersHost = nullptr;    // R15 (PP04): list + honest notice host
    QLabel* m_layersNote = nullptr;     // R15 (PP04): visible visibility disclosure

    AnnotationItem m_activeAnnotation;
};

} // namespace gp
