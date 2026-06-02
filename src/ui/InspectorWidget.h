// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include "core/AnnotationTypes.h"

class QStackedWidget;
class QButtonGroup;
class QLabel;
class QScrollArea;
class QPdfDocument;
class QDoubleSpinBox;
class QSlider;
class QComboBox;
class QSpinBox;
class QTextEdit;
class QListWidget;
class QPushButton;
class PdfViewerWidget;
struct AppContext;

class InspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit InspectorWidget(QWidget* parent = nullptr);
    void init(const AppContext* ctx, PdfViewerWidget* viewer);
    void setAnnotation(const AnnotationItem* item);
    void clearAnnotation();

    void setDocument(QPdfDocument* doc, const QString& path);
    void setCurrentPage(int page);
    void showPropertiesTab();
    void showSignaturesTab();

signals:
    void annotationModified();

private:
    void createTabs();
    QWidget* createEmptyState();
    QWidget* createPropertiesView();
    QWidget* createDocInfoView();
    QWidget* createCollapsibleSection(const QString& title, QWidget* content);
    void refreshProperties();
    void refreshDocInfo();
    void pushEditCommand(const AnnotationItem& newAnn);
    bool eventFilter(QObject* obj, QEvent* event) override;

    // ── Shell ─────────────────────────────────────────────────────────────
    QWidget*           m_tabBar        = nullptr;
    QButtonGroup*      m_tabGroup      = nullptr;
    QStackedWidget*    m_contentStack  = nullptr;  // 0=empty, 1=props, 2=docinfo
    QScrollArea*       m_propsScroll   = nullptr;
    QWidget*           m_propsContent  = nullptr;
    QWidget*           m_headerWidget  = nullptr;
    QWidget*           m_docInfoContent = nullptr;

    // ── D1: Identity field values ──────────────────────────────────────────
    QLabel*            m_valAuthor     = nullptr;
    QLabel*            m_valCreated    = nullptr;
    QLabel*            m_valModified   = nullptr;
    QLabel*            m_valSubject    = nullptr;
    QLabel*            m_valAnnotId    = nullptr;

    // ── D2: Geometry spinboxes ─────────────────────────────────────────────
    QDoubleSpinBox*    m_spinX         = nullptr;
    QDoubleSpinBox*    m_spinY         = nullptr;
    QDoubleSpinBox*    m_spinW         = nullptr;
    QDoubleSpinBox*    m_spinH         = nullptr;

    // ── D3: Appearance controls ────────────────────────────────────────────
    QSlider*           m_opacitySlider = nullptr;
    QLabel*            m_opacityLabel  = nullptr;
    QComboBox*         m_blendCombo    = nullptr;
    QSpinBox*          m_borderSpin    = nullptr;

    // ── D4: Contents editor (M6-P4: Djot-aware) ────────────────────────────
    QTextEdit*         m_contentsEditor = nullptr;  // Djot source input
    QTextEdit*         m_djotPreview    = nullptr;  // live HTML render (read-only)
    QLabel*            m_charCountLabel = nullptr;

    // M6-P4 D2 helpers — Djot toolbar actions + live preview refresh
    void wrapSelection(const QString& prefix, const QString& suffix);
    void insertLinePrefix(const QString& prefix);
    void refreshDjotPreview();
    void commitDjotEdit();

    // ── D5: Reply thread ──────────────────────────────────────────────────
    QListWidget*       m_replyList     = nullptr;
    QTextEdit*         m_replyEditor   = nullptr;
    QPushButton*       m_replySubmit   = nullptr;
    QWidget*           m_replyEditorWidget = nullptr;

    // ── State ─────────────────────────────────────────────────────────────
    const AnnotationItem* m_currentAnnotation = nullptr;
    QPdfDocument*      m_pdfDocument  = nullptr;
    QString            m_documentPath;
    int                m_currentPageNum = 0;
    const AppContext*  m_ctx = nullptr;
    PdfViewerWidget*   m_viewer = nullptr;
    bool               m_refreshing = false;  // guard against re-entrant signals
};
