#include "ui/CompareWidget.h"
#include "ui/PdfViewerWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QTextBrowser>
#include <QLabel>
#include <QTimer>

// ---------------------------------------------------------------------------
// Colour constants for the diff display
// ---------------------------------------------------------------------------
static constexpr const char* CLR_DEL  = "#cc3333";   // red   — deleted token
static constexpr const char* CLR_ADD  = "#2d9e2d";   // green — added token
static constexpr const char* CLR_MOV  = "#d97c00";   // orange — moved token
static constexpr const char* CLR_KEEP = "#888888";   // grey  — unchanged

CompareWidget::CompareWidget(QWidget *parent)
    : QWidget(parent)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ── Top: side-by-side PDF viewers ──────────────────────────────────
    QSplitter* pdfSplit = new QSplitter(Qt::Horizontal, this);
    m_viewerLeft  = new PdfViewerWidget(this);
    m_viewerRight = new PdfViewerWidget(this);
    pdfSplit->addWidget(m_viewerLeft);
    pdfSplit->addWidget(m_viewerRight);
    col->addWidget(pdfSplit, 3);  // 75% of space

    // ── Bottom: text diff panel ────────────────────────────────────────
    // Navigation bar
    auto* navBar = new QWidget(this);
    navBar->setFixedHeight(24);
    navBar->setStyleSheet("background:#1e1f22; border-top:1px solid #393b40;");
    auto* navRow = new QHBoxLayout(navBar);
    navRow->setContentsMargins(8, 0, 8, 0);
    navRow->setSpacing(6);
    auto* navTitle = new QLabel("TEXT DIFF", navBar);
    navTitle->setStyleSheet("color:#71747a; font-size:10px; font-family:monospace; font-weight:bold;");
    navRow->addWidget(navTitle);
    navRow->addStretch(1);
    m_navLabel = new QLabel("", navBar);
    m_navLabel->setStyleSheet("color:#71747a; font-size:10px; font-family:monospace;");
    navRow->addWidget(m_navLabel);
    col->addWidget(navBar);

    m_textDiff = new QTextBrowser(this);
    m_textDiff->setMinimumHeight(80);
    m_textDiff->setOpenLinks(false);
    m_textDiff->setStyleSheet(
        "QTextBrowser { background:#1a1b1e; color:#dfe1e5; "
        "font-family:monospace; font-size:11px; border:none; padding:4px; }");
    col->addWidget(m_textDiff, 1);  // 25% of space

    // Sync viewer scrollbars
    QTimer::singleShot(100, this, [this]() {
        auto* v1 = m_viewerLeft->findChild<QAbstractScrollArea*>();
        auto* v2 = m_viewerRight->findChild<QAbstractScrollArea*>();
        if (v1 && v2) {
            connect(v1->verticalScrollBar(), &QScrollBar::valueChanged,
                    v2->verticalScrollBar(), &QScrollBar::setValue);
            connect(v2->verticalScrollBar(), &QScrollBar::valueChanged,
                    v1->verticalScrollBar(), &QScrollBar::setValue);
        }
    });
}

bool CompareWidget::loadDocuments(const QString& file1, const QString& file2)
{
    return m_viewerLeft->loadDocument(file1) && m_viewerRight->loadDocument(file2);
}

void CompareWidget::setDiffResult(const DiffResult& result)
{
    m_diffResult = result;
    buildTextDiff();
}

void CompareWidget::setShowPixelDiff(bool show)
{
    m_showPixelDiff = show;
    if (m_showPixelDiff && !m_diffResult.pages.isEmpty())
        m_viewerRight->setOverlayImage(m_diffResult.pages.first().diffImage);
    else
        m_viewerRight->setOverlayImage(QImage());
}

// ---------------------------------------------------------------------------
// Text diff display
// ---------------------------------------------------------------------------

void CompareWidget::buildTextDiff()
{
    m_anchors.clear();
    m_currentAnchor = -1;
    m_textDiff->setHtml(buildHtml());

    const int totalAnchors = m_anchors.size();
    if (totalAnchors == 0) {
        m_navLabel->setText("no text changes");
    } else {
        m_navLabel->setText(
            QString("%1 change(s)  |  ← → to navigate").arg(totalAnchors));
    }
}

QString CompareWidget::buildHtml() const
{
    if (m_diffResult.isIdentical)
        return QStringLiteral("<span style='color:#4ec96d'>Files are identical.</span>");

    QString html;
    html.reserve(4096);
    html += "<style>body{font-family:monospace;font-size:11px;color:#dfe1e5;background:#1a1b1e;}</style>";

    int anchorIdx = 0;

    for (const PageDiff& page : m_diffResult.pages) {
        const bool hasText  = !page.textAdded.isEmpty() || !page.textRemoved.isEmpty()
                              || !page.moves.isEmpty();
        const bool hasPixel = page.pixelDiffCount > 0;
        if (!hasText && !hasPixel) continue;

        html += QString("<p><b style='color:#a8abb0'>Page %1</b></p>").arg(page.pageIndex + 1);

        // Moves (orange)
        for (const MoveOperation& mv : page.moves) {
            const QString aid = QString("chg%1").arg(anchorIdx++);
            const_cast<CompareWidget*>(this)->m_anchors.append(aid);
            html += QString("<p><a name='%1'/>"
                            "<span style='color:%2'>&#x2194;</span> "
                            "<span style='color:%2;text-decoration:underline'>%3</span> "
                            "<span style='color:%4'>[moved from pos&nbsp;%5 → %6]</span></p>")
                        .arg(aid, CLR_MOV,
                             mv.token.toHtmlEscaped(),
                             CLR_KEEP)
                        .arg(mv.fromIndex)
                        .arg(mv.toIndex);
        }

        // Additions (green)
        for (const QString& tok : page.textAdded) {
            const QString aid = QString("chg%1").arg(anchorIdx++);
            const_cast<CompareWidget*>(this)->m_anchors.append(aid);
            html += QString("<p><a name='%1'/>"
                            "<span style='color:%2'>+</span> "
                            "<span style='color:%2'>%3</span></p>")
                        .arg(aid, CLR_ADD, tok.toHtmlEscaped());
        }

        // Deletions (red)
        for (const QString& tok : page.textRemoved) {
            const QString aid = QString("chg%1").arg(anchorIdx++);
            const_cast<CompareWidget*>(this)->m_anchors.append(aid);
            html += QString("<p><a name='%1'/>"
                            "<span style='color:%2'>−</span> "
                            "<span style='color:%2;text-decoration:line-through'>%3</span></p>")
                        .arg(aid, CLR_DEL, tok.toHtmlEscaped());
        }

        if (hasPixel) {
            html += QString("<p><span style='color:#888'>~ pixel diff: %1 px changed</span></p>")
                        .arg(page.pixelDiffCount);
        }
    }

    if (html.endsWith("<style>body{font-family:monospace;font-size:11px;color:#dfe1e5;background:#1a1b1e;}</style>"))
        html += "<span style='color:#4ec96d'>No text changes detected.</span>";

    return html;
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void CompareWidget::nextChange()
{
    if (m_anchors.isEmpty()) return;
    m_currentAnchor = (m_currentAnchor + 1) % m_anchors.size();
    m_textDiff->scrollToAnchor(m_anchors[m_currentAnchor]);
    m_navLabel->setText(
        QString("change %1 of %2  |  ← → to navigate")
            .arg(m_currentAnchor + 1)
            .arg(m_anchors.size()));
}

void CompareWidget::prevChange()
{
    if (m_anchors.isEmpty()) return;
    m_currentAnchor = (m_currentAnchor - 1 + m_anchors.size()) % m_anchors.size();
    m_textDiff->scrollToAnchor(m_anchors[m_currentAnchor]);
    m_navLabel->setText(
        QString("change %1 of %2  |  ← → to navigate")
            .arg(m_currentAnchor + 1)
            .arg(m_anchors.size()));
}

