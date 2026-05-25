#pragma once
#include <QFrame>
#include "core/AnnotationTypes.h"

class QStackedWidget;
class QTabBar;
class QTreeView;
class QListWidget;
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

    AnnotationItem m_activeAnnotation;
};

} // namespace gp
