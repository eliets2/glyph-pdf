#pragma once
#include <QWidget>
#include "core/AnnotationTypes.h"

class QStackedWidget;
class QButtonGroup;
class QLabel;
class QScrollArea;
class QPdfDocument;

class InspectorWidget : public QWidget {
    Q_OBJECT
public:
    explicit InspectorWidget(QWidget* parent = nullptr);
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

    QWidget*           m_tabBar;
    QButtonGroup*      m_tabGroup;
    QStackedWidget*    m_contentStack;     // 0=empty, 1=annotation props, 2=doc info
    QScrollArea*       m_propsScroll;
    QWidget*           m_propsContent;
    QWidget*           m_headerWidget;
    QWidget*           m_docInfoContent = nullptr;
    const AnnotationItem* m_currentAnnotation = nullptr;
    QPdfDocument*      m_pdfDocument  = nullptr;
    QString            m_documentPath;
    int                m_currentPageNum = 0;
};
