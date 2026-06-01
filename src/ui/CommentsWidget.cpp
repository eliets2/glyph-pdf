#include "ui/CommentsWidget.h"
#include "ui/PdfViewerWidget.h"
#include "core/AnnotationTypes.h"
#include "commands/EditAnnotationCommand.h"
#include "pdfws_djot/DjotToRichTextXhtml.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QUuid>
#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QPixmap>
#include <QHash>

namespace {

static const QColor kAvatarPalette[] = {
    QColor(0x3B, 0x82, 0xF6), // blue
    QColor(0xEF, 0x44, 0x44), // red
    QColor(0x10, 0xB9, 0x81), // emerald
    QColor(0xF5, 0x9E, 0x0B), // amber
    QColor(0x8B, 0x5C, 0xF6), // violet
    QColor(0xEC, 0x48, 0x99), // pink
    QColor(0x06, 0xB6, 0xD4), // cyan
    QColor(0xF9, 0x73, 0x16), // orange
};
constexpr int kPaletteSize = sizeof(kAvatarPalette) / sizeof(kAvatarPalette[0]);

QPixmap generateAvatar(const QString &authorName, int size = 28)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // Deterministic color from author name
    uint hash = qHash(authorName);
    QColor bg = kAvatarPalette[hash % kPaletteSize];

    // Draw circle
    p.setBrush(bg);
    p.setPen(Qt::NoPen);
    p.drawEllipse(0, 0, size, size);

    // Draw initial letter
    QChar letter = authorName.isEmpty() ? QChar('?') : authorName.at(0).toUpper();
    p.setPen(Qt::white);
    QFont f;
    f.setPixelSize(size * 0.52);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, QString(letter));
    p.end();

    return pm;
}

QString reviewStateLabel(ReviewState s)
{
    switch (s) {
    case ReviewState::Open:      return QObject::tr("Open");
    case ReviewState::Accepted:  return QObject::tr("Accepted");
    case ReviewState::Rejected:  return QObject::tr("Rejected");
    case ReviewState::Completed: return QObject::tr("Completed");
    case ReviewState::Cancelled: return QObject::tr("Cancelled");
    default:                     return QObject::tr("None");
    }
}

QColor reviewStateColor(ReviewState s)
{
    switch (s) {
    case ReviewState::Open:      return QColor(0x3B, 0x82, 0xF6);
    case ReviewState::Accepted:  return QColor(0x10, 0xB9, 0x81);
    case ReviewState::Rejected:  return QColor(0xEF, 0x44, 0x44);
    case ReviewState::Completed: return QColor(0x8B, 0x5C, 0xF6);
    case ReviewState::Cancelled: return QColor(0x94, 0xA3, 0xB8);
    default:                     return QColor(0x64, 0x74, 0x8B);
    }
}

} // anonymous namespace

CommentsWidget::CommentsWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Thread"), this);
    title->setStyleSheet("font-weight: 700; font-size: 11px; color: #94A3B8; text-transform: uppercase; letter-spacing: 0.6px;");
    layout->addWidget(title);

    auto *filterLayout = new QHBoxLayout();
    m_filterStatus = new QComboBox(this);
    m_filterStatus->addItems({"All", "Open", "Accepted", "Rejected", "Completed"});
    m_filterAuthor = new QComboBox(this);
    m_filterAuthor->addItem("All Authors");
    filterLayout->addWidget(m_filterStatus);
    filterLayout->addWidget(m_filterAuthor);
    layout->addLayout(filterLayout);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setWordWrap(true);
    layout->addWidget(m_tree, 1);

    auto *composer = new QWidget(this);
    composer->setObjectName("composer");
    composer->setStyleSheet("#composer { background: #1E293B; border-radius: 6px; padding: 8px; }");
    auto *cLyt = new QVBoxLayout(composer);
    cLyt->setContentsMargins(8,8,8,8);
    cLyt->setSpacing(6);

    m_author = new QLineEdit(this);
    m_author->setPlaceholderText(tr("Your Name"));
    m_author->setStyleSheet("QLineEdit { background: #0F172A; border: 1px solid #334155; border-radius: 4px; padding: 4px; color: #F8FAFC; }");
    cLyt->addWidget(m_author);

    m_editor = new QTextEdit(this);
    m_editor->setPlaceholderText(tr("Add a comment or reply..."));
    m_editor->setFixedHeight(60);
    m_editor->setStyleSheet("QTextEdit { background: #0F172A; border: 1px solid #334155; border-radius: 4px; padding: 4px; color: #F8FAFC; }");
    cLyt->addWidget(m_editor);

    auto *btnLyt = new QHBoxLayout;
    btnLyt->addStretch();
    m_addBtn = new QPushButton(tr("Post"), this);
    m_addBtn->setStyleSheet("QPushButton { background: #3B82F6; color: white; border: none; border-radius: 4px; padding: 4px 12px; font-weight: 600; } QPushButton:hover { background: #2563EB; }");
    btnLyt->addWidget(m_addBtn);
    
    auto *replyBtn = new QPushButton(tr("Reply"), this);
    replyBtn->setStyleSheet("QPushButton { background: #10B981; color: white; border: none; border-radius: 4px; padding: 4px 12px; font-weight: 600; } QPushButton:hover { background: #059669; }");
    btnLyt->addWidget(replyBtn);

    cLyt->addLayout(btnLyt);
    layout->addWidget(composer);

    connect(m_addBtn, &QPushButton::clicked, this, &CommentsWidget::addComment);
    connect(replyBtn, &QPushButton::clicked, this, &CommentsWidget::replyToComment);
    connect(m_filterStatus, &QComboBox::currentTextChanged, this, &CommentsWidget::refreshList);
    connect(m_filterAuthor, &QComboBox::currentTextChanged, this, &CommentsWidget::refreshList);

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        auto *item = m_tree->itemAt(pos);
        if (!item) return;
        QString annoId = item->data(0, Qt::UserRole).toString();
        if (annoId.isEmpty()) return;

        QMenu menu(this);
        menu.setStyleSheet("QMenu { background: #1E293B; color: #F8FAFC; border: 1px solid #334155; }"
                           "QMenu::item:selected { background: #3B82F6; }");

        auto *actOpen      = menu.addAction(tr("Mark Open"));
        auto *actAccepted   = menu.addAction(tr("Mark Accepted"));
        auto *actRejected   = menu.addAction(tr("Mark Rejected"));
        auto *actCompleted  = menu.addAction(tr("Mark Completed"));
        auto *actCancelled  = menu.addAction(tr("Mark Cancelled"));

        auto setReviewState = [this, annoId](ReviewState newState) {
            if (!m_viewer) return;
            auto annos = m_viewer->annotations();
            for (int i = 0; i < annos.size(); ++i) {
                if (annos[i].id == annoId) {
                    annos[i].reviewState = newState;
                    m_viewer->setAnnotations(annos);
                    reloadAnnotations();
                    return;
                }
            }
        };

        connect(actOpen,     &QAction::triggered, this, [=]{ setReviewState(ReviewState::Open); });
        connect(actAccepted,  &QAction::triggered, this, [=]{ setReviewState(ReviewState::Accepted); });
        connect(actRejected,  &QAction::triggered, this, [=]{ setReviewState(ReviewState::Rejected); });
        connect(actCompleted, &QAction::triggered, this, [=]{ setReviewState(ReviewState::Completed); });
        connect(actCancelled, &QAction::triggered, this, [=]{ setReviewState(ReviewState::Cancelled); });

        menu.exec(m_tree->viewport()->mapToGlobal(pos));
    });

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int column) {
        Q_UNUSED(column)
        bool ok;
        int p = item->data(0, Qt::UserRole + 1).toInt(&ok);
        if (ok && p >= 0) {
            emit commentDoubleClicked(p);
        }
    });
}

void CommentsWidget::setViewer(PdfViewerWidget *viewer)
{
    m_viewer = viewer;
    reloadAnnotations();
}

void CommentsWidget::setDocumentFile(const QString &filePath)
{
    m_filePath = filePath;
}

void CommentsWidget::setCurrentPage(int page)
{
    m_currentPage = page;
}

void CommentsWidget::reloadAnnotations()
{
    if (m_viewer) {
        buildTree(m_viewer->annotations());
    }
}

void CommentsWidget::refreshList()
{
    reloadAnnotations();
}

void CommentsWidget::buildTree(const QList<AnnotationItem> &items)
{
    m_tree->clear();
    m_filterAuthor->blockSignals(true);
    QString currentAuthor = m_filterAuthor->currentText();
    m_filterAuthor->clear();
    m_filterAuthor->addItem("All Authors");
    
    QSet<QString> authors;
    QMap<QString, QTreeWidgetItem*> itemMap;

    QString statusFilter = m_filterStatus->currentText();
    bool filterAllStatus = statusFilter == "All";

    for (const auto &anno : items) {
        if (anno.mode != ToolMode::AddComment) continue;
        if (!anno.author.isEmpty()) authors.insert(anno.author);
        
        bool statusMatch = filterAllStatus;
        if (!statusMatch) {
            if (statusFilter == "Open" && anno.reviewState == ReviewState::Open) statusMatch = true;
            else if (statusFilter == "Accepted" && anno.reviewState == ReviewState::Accepted) statusMatch = true;
            else if (statusFilter == "Rejected" && anno.reviewState == ReviewState::Rejected) statusMatch = true;
            else if (statusFilter == "Completed" && anno.reviewState == ReviewState::Completed) statusMatch = true;
        }

        bool authorMatch = (currentAuthor == "All Authors" || currentAuthor == anno.author);

        if (statusMatch && authorMatch) {
            auto *node = new QTreeWidgetItem();

            // Generate circular avatar icon
            QPixmap avatar = generateAvatar(anno.author);
            node->setIcon(0, QIcon(avatar));

            // Build rich display text with review state badge
            QString stateTag = reviewStateLabel(anno.reviewState);
            QString display = QString("%1  \u2022  %2  [%3]\n%4")
                .arg(anno.author, anno.creationDate, stateTag, anno.text);
            node->setText(0, display);

            // Set foreground color for the state
            node->setForeground(0, QBrush(QColor(0xF8, 0xFA, 0xFC)));

            node->setData(0, Qt::UserRole, anno.id);
            node->setData(0, Qt::UserRole + 1, anno.pageIndex);
            node->setData(0, Qt::UserRole + 2, static_cast<int>(anno.reviewState));
            itemMap.insert(anno.id, node);
        }
    }

    for (const auto &anno : items) {
        if (anno.mode != ToolMode::AddComment) continue;
        if (itemMap.contains(anno.id)) {
            if (!anno.parentId.isEmpty() && itemMap.contains(anno.parentId)) {
                itemMap[anno.parentId]->addChild(itemMap[anno.id]);
            } else {
                m_tree->addTopLevelItem(itemMap[anno.id]);
            }
        }
    }

    for (const QString &a : authors) {
        m_filterAuthor->addItem(a);
    }
    m_filterAuthor->setCurrentText(currentAuthor);
    m_filterAuthor->blockSignals(false);
    m_tree->expandAll();
}

void CommentsWidget::addComment()
{
    if (!m_viewer) return;
    QString author = m_author->text().trimmed();
    QString content = m_editor->toPlainText().trimmed();
    if (author.isEmpty() || content.isEmpty()) return;

    AnnotationItem anno;
    anno.id = QUuid::createUuid().toString();
    anno.mode = ToolMode::AddComment;
    anno.pageIndex = qMax(0, m_currentPage - 1);
    anno.author = author;
    // M6-P4 D6: comments share the Djot rich-text model. The composer is still
    // plain text, so the entered text is the (trivial) Djot source; the shared
    // PoDoFoBackend::embedAnnotations dual-write then emits /Contents + /RC +
    // /PieceInfo for comments exactly as for InspectorWidget annotations.
    // TODO(M6-P5): give the comment composer the same Djot formatting toolbar +
    // live preview as InspectorWidget so replies can be authored with markup.
    anno.djotSource = content;
    anno.text = pdfws_djot::djotToPlainText(content);
    anno.creationDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    anno.reviewState = ReviewState::Open;
    anno.color = Qt::yellow;
    anno.rect = QRectF(50, 50, 24, 24); // default pos

    QList<AnnotationItem> annos = m_viewer->annotations();
    annos.append(anno);
    m_viewer->setAnnotations(annos);

    m_editor->clear();
    reloadAnnotations();
}

void CommentsWidget::replyToComment()
{
    if (!m_viewer) return;
    auto *sel = m_tree->currentItem();
    if (!sel) return;

    QString parentId = sel->data(0, Qt::UserRole).toString();
    QString author = m_author->text().trimmed();
    QString content = m_editor->toPlainText().trimmed();
    if (author.isEmpty() || content.isEmpty()) return;

    AnnotationItem anno;
    anno.id = QUuid::createUuid().toString();
    anno.parentId = parentId;
    anno.mode = ToolMode::AddComment;
    anno.pageIndex = sel->data(0, Qt::UserRole + 1).toInt();
    anno.author = author;
    // M6-P4 D6: replies use the same Djot rich-text dual-write as top-level
    // comments (see addComment). TODO(M6-P5): rich Djot composer for replies.
    anno.djotSource = content;
    anno.text = pdfws_djot::djotToPlainText(content);
    anno.creationDate = QDateTime::currentDateTime().toString(Qt::ISODate);
    anno.reviewState = ReviewState::Open;

    QList<AnnotationItem> annos = m_viewer->annotations();
    
    // Add reply ID to parent
    for (int i = 0; i < annos.size(); ++i) {
        if (annos[i].id == parentId) {
            annos[i].replies.append(anno.id);
            break;
        }
    }
    
    annos.append(anno);
    m_viewer->setAnnotations(annos);

    m_editor->clear();
    reloadAnnotations();
}

void CommentsWidget::changeReviewState()
{
    // Not fully wired yet, but prepared for context menu actions
}
