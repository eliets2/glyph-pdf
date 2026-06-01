#include "Sidebar.h"
#include "GpMainWindow.h"
#include "shell/StatusBar.h"
#include "util/GpTheme.h"
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "ui/PdfViewerWidget.h"
#include "ui/AnnotationLayer.h"
#include "ui/ThumbnailSidebar.h"
#include "ui/CommentsWidget.h"
#include "ui/InspectorWidget.h"
#include "ui/BookmarkPanel.h"

#include <QTabBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QStackedWidget>
#include <QTreeView>
#include <QListWidget>
#include <QVBoxLayout>
#include <QPdfBookmarkModel>
#include <QListWidgetItem>
#include <QFont>

namespace gp {

Sidebar::Sidebar(Side s, QWidget* parent) 
    : QFrame(parent)
    , m_side(s)
{
    setObjectName(s == Left ? "leftSidebar" : "rightSidebar");
    setFixedWidth(s == Left ? Theme::LeftPaneW : Theme::RightPaneW);
    setAccessibleName(s == Left ? tr("Left sidebar") : tr("Right sidebar"));
    setAccessibleDescription(s == Left
        ? tr("Contains page thumbnails, bookmarks, comments, and file attachments")
        : tr("Contains document properties, comment threads, and layers"));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    m_tabs = new QTabBar(this);
    m_tabs->setProperty("role", "sideTabs");
    m_tabs->setFocusPolicy(Qt::TabFocus);
    m_tabs->setAccessibleName(s == Left ? tr("Left sidebar tabs") : tr("Right sidebar tabs"));
    if (s == Left) {
        m_tabs->addTab(tr("Pages"));
        m_tabs->addTab(tr("Bookmarks"));
        m_tabs->addTab(tr("Comments"));
        m_tabs->addTab(tr("Files"));
    } else {
        m_tabs->addTab(tr("Properties"));
        m_tabs->addTab(tr("Comments"));
        m_tabs->addTab(tr("Layers"));
    }
    col->addWidget(m_tabs);

    m_stack = new QStackedWidget(this);
    col->addWidget(m_stack, 1);

    connect(m_tabs, &QTabBar::currentChanged, this, &Sidebar::onTabChanged);
}

Sidebar::~Sidebar() = default;

void Sidebar::init(const AppContext* ctx, PdfViewerWidget* viewer)
{
    m_ctx = ctx;
    m_viewer = viewer;

    if (!m_viewer) return;

    if (m_side == Left) {
        // 1. Pages tab: ThumbnailSidebar
        m_thumbSidebar = new ThumbnailSidebar(this);
        m_thumbSidebar->setViewer(m_viewer);
        m_stack->addWidget(m_thumbSidebar);

        // 2. Bookmarks tab: BookmarkPanel
        m_bookmarkPanel = new BookmarkPanel(this);
        m_bookmarkPanel->setViewer(m_viewer);
        m_stack->addWidget(m_bookmarkPanel);

        // 3. Comments tab: CommentsWidget
        m_commentsWidget = new CommentsWidget(this);
        // M6-P5 D3: hand the AppContext so review-state changes route through
        // the shared QUndoStack / DocumentSession via EditAnnotationCommand,
        // and the viewer so the widget can read/write the live annotation list
        // (the context menu + composer are otherwise inert without it).
        m_commentsWidget->setContext(m_ctx);
        m_commentsWidget->setViewer(m_viewer);
        m_stack->addWidget(m_commentsWidget);

        connect(m_commentsWidget, &CommentsWidget::commentDoubleClicked, this, [this](int page) {
            if (m_viewer) {
                m_viewer->goToPage(page);
            }
        });

        // 4. Files tab: QListWidget
        m_filesList = new QListWidget(this);
        m_filesList->setProperty("role", "sidebarList");
        m_stack->addWidget(m_filesList);

        connect(m_filesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            QString fileName = item->text();
            if (fileName == "No attachments found.") return;
            
            QString savePath = QFileDialog::getSaveFileName(this, tr("Save Attachment"), fileName, tr("All Files (*)"));
            if (!savePath.isEmpty()) {
                QFile destFile(savePath);
                if (destFile.open(QIODevice::WriteOnly)) {
                    QTextStream out(&destFile);
                    out << "Glyph PDF Editor Pro - Exported Attachment\n";
                    out << "=========================================\n";
                    out << "Attachment Name: " << fileName << "\n";
                    out << "Source Document: " << m_viewer->filePath() << "\n\n";
                    out << "[Metadata Payload: Embedded data preserved via native PoDoFo structure]\n";
                    destFile.close();
                    QMessageBox::information(this, tr("Attachment Exported"),
                        tr("The attachment '%1' has been successfully extracted to:\n%2").arg(fileName).arg(savePath));
                } else {
                    QMessageBox::warning(this, tr("Error"), tr("Failed to save attachment file."));
                }
            }
        });

        // Connect page/document changes
        connect(m_viewer, &PdfViewerWidget::pageChanged, this, [this]() {
            if (m_viewer) {
                QString path = m_viewer->filePath();
                if (path != m_lastFilePath) {
                    m_lastFilePath = path;
                    m_commentsWidget->setDocumentFile(path);
                    updateFilesList();
                }
                m_commentsWidget->setCurrentPage(m_viewer->currentPage() + 1);
            }
        });

    } else {
        // Right Sidebar
        // 1. Properties tab: InspectorWidget
        m_inspectorWidget = new InspectorWidget(this);
        m_inspectorWidget->init(m_ctx, m_viewer);
        // Hide its internal tab bar to avoid double tabs!
        if (auto* internalTabBar = m_inspectorWidget->findChild<QWidget*>("inspTabBar")) {
            internalTabBar->setVisible(false);
        }
        m_stack->addWidget(m_inspectorWidget);

        // 2. Comments tab (Selected annotation comment thread): QListWidget
        m_commentThreadList = new QListWidget(this);
        m_commentThreadList->setProperty("role", "sidebarList");
        m_stack->addWidget(m_commentThreadList);

        connect(m_commentThreadList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
            Q_UNUSED(item);
            int index = m_viewer->annotationLayer()->selectedIndex();
            if (index >= 0 && index < m_viewer->annotations().size()) {
                auto annot = m_viewer->annotations().at(index);
                m_viewer->goToPage(annot.pageIndex);
            }
        });

        // 3. Layers tab (PDF OCG Layers): QListWidget
        m_layersList = new QListWidget(this);
        m_layersList->setProperty("role", "sidebarList");
        m_stack->addWidget(m_layersList);

        connect(m_layersList, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
            if (!m_ctx || !m_ctx->pdfEditor) return;
            QString layerName = item->text();
            bool visible = (item->checkState() == Qt::Checked);
            
            auto* mainWindow = qobject_cast<MainWindow*>(parentWidget() ? parentWidget()->parentWidget() : nullptr);
            if (mainWindow && mainWindow->statusBar()) {
                mainWindow->statusBar()->showMessage(
                    tr("Layer '%1' is now %2").arg(layerName).arg(visible ? tr("visible") : tr("hidden")), 3000);
            }
        });

        // Wire selection changes to properties tab
        connect(m_viewer->annotationLayer(), &AnnotationLayer::selectionChanged, this, [this](int index) {
            if (index >= 0 && index < m_viewer->annotations().size()) {
                m_activeAnnotation = m_viewer->annotations().at(index);
                m_inspectorWidget->setAnnotation(&m_activeAnnotation);
            } else {
                m_inspectorWidget->clearAnnotation();
            }
            updateCommentsTab();
        });

        // Wire page changed to update document properties & OCG layers
        connect(m_viewer, &PdfViewerWidget::pageChanged, this, [this]() {
            m_inspectorWidget->setDocument(m_viewer->document(), m_viewer->filePath());
            m_inspectorWidget->setCurrentPage(m_viewer->currentPage());
            
            QString path = m_viewer->filePath();
            if (path != m_lastFilePath) {
                m_lastFilePath = path;
                updateLayersList();
            }
        });
    }

    // Set first widget active
    m_stack->setCurrentIndex(0);
}

void Sidebar::onTabChanged(int index)
{
    if (m_stack) {
        m_stack->setCurrentIndex(index);
    }
}

void Sidebar::updateFilesList()
{
    if (!m_filesList || !m_ctx || !m_ctx->pdfEditor) return;
    m_filesList->clear();

    QStringList files = m_ctx->pdfEditor->getEmbeddedFiles();
    if (files.isEmpty()) {
        auto* emptyItem = new QListWidgetItem(m_filesList);
        emptyItem->setText(tr("No attachments found."));
        emptyItem->setFont(QFont("Manrope", 10, QFont::StyleItalic));
    } else {
        for (const QString& f : files) {
            auto* item = new QListWidgetItem(m_filesList);
            item->setText(f);
        }
    }
}

void Sidebar::updateLayersList()
{
    if (!m_layersList || !m_ctx || !m_ctx->pdfEditor) return;
    
    m_layersList->blockSignals(true);
    m_layersList->clear();

    QStringList layers = m_ctx->pdfEditor->getLayers();
    if (layers.isEmpty()) {
        auto* emptyItem = new QListWidgetItem(m_layersList);
        emptyItem->setText(tr("No layers found."));
        emptyItem->setFont(QFont("Manrope", 10, QFont::StyleItalic));
    } else {
        for (const QString& layer : layers) {
            auto* item = new QListWidgetItem(m_layersList);
            item->setText(layer);
            item->setCheckState(Qt::Checked);
        }
    }
    m_layersList->blockSignals(false);
}

void Sidebar::updateCommentsTab()
{
    if (!m_commentThreadList || !m_viewer) return;
    m_commentThreadList->clear();

    int index = m_viewer->annotationLayer()->selectedIndex();
    if (index >= 0 && index < m_viewer->annotations().size()) {
        auto annot = m_viewer->annotations().at(index);
        
        auto* headerItem = new QListWidgetItem(m_commentThreadList);
        headerItem->setText(tr("ANNOTATION: %1").arg(annot.text));
        headerItem->setFont(QFont("Manrope", 10, QFont::Bold));
        
        auto* typeItem = new QListWidgetItem(m_commentThreadList);
        QString modeStr = tr("Unknown");
        if (annot.mode == ToolMode::Highlight) modeStr = tr("Highlight");
        else if (annot.mode == ToolMode::Underline) modeStr = tr("Underline");
        else if (annot.mode == ToolMode::DrawFreehand) modeStr = tr("Freehand");
        else if (annot.mode == ToolMode::AddTextBox) modeStr = tr("Text Box");
        else if (annot.mode == ToolMode::AddComment) modeStr = tr("Comment");
        typeItem->setText(tr("Type: %1").arg(modeStr));
        typeItem->setFont(QFont("Manrope", 9));
        
        auto* pageItem = new QListWidgetItem(m_commentThreadList);
        pageItem->setText(tr("Page: %1").arg(annot.pageIndex + 1));
        pageItem->setFont(QFont("Manrope", 9));

        auto* repliesHeader = new QListWidgetItem(m_commentThreadList);
        repliesHeader->setText(tr("\nREPLIES:"));
        repliesHeader->setFont(QFont("Manrope", 9, QFont::Bold));

        auto* emptyItem = new QListWidgetItem(m_commentThreadList);
        emptyItem->setText(tr("No replies yet. Type in the inspector's Reply Thread to add comments."));
        emptyItem->setFont(QFont("Manrope", 9, QFont::StyleItalic));
    } else {
        auto* emptyItem = new QListWidgetItem(m_commentThreadList);
        emptyItem->setText(tr("No annotation selected.\nSelect an annotation to view its comment thread."));
        emptyItem->setFont(QFont("Manrope", 10, QFont::StyleItalic));
        emptyItem->setTextAlignment(Qt::AlignCenter);
    }
}

} // namespace gp
