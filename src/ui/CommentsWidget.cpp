#include "ui/CommentsWidget.h"

#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

CommentsWidget::CommentsWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("Thread"), this);
    title->setStyleSheet("font-weight: 700; font-size: 11px; color: #94A3B8; text-transform: uppercase; letter-spacing: 0.6px;");
    layout->addWidget(title);

    m_list = new QListWidget(this);
    m_list->setObjectName("commentsList");
    m_list->setWordWrap(true);
    layout->addWidget(m_list, 1);

    auto *composer = new QWidget(this);
    auto *composerLayout = new QVBoxLayout(composer);
    composerLayout->setContentsMargins(0, 0, 0, 0);
    composerLayout->setSpacing(6);

    auto *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);

    m_author = new QLineEdit(this);
    m_author->setPlaceholderText(tr("Author"));
    m_author->setText(tr("Current User"));
    row->addWidget(m_author, 1);

    m_addBtn = new QPushButton(tr("Add"), this);
    m_addBtn->setProperty("primary", true);
    row->addWidget(m_addBtn, 0);

    composerLayout->addLayout(row);

    m_editor = new QTextEdit(this);
    m_editor->setPlaceholderText(tr("Add a comment..."));
    m_editor->setFixedHeight(68);
    composerLayout->addWidget(m_editor);

    layout->addWidget(composer, 0);

    connect(m_addBtn, &QPushButton::clicked, this, &CommentsWidget::addComment);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        int index = m_list->row(item);
        if (index >= 0 && index < m_comments.size()) {
            const auto &c = m_comments.at(index);
            if (c.page > 0) {
                emit commentDoubleClicked(c.page - 1);
            }
        }
    });
}

void CommentsWidget::setDocumentFile(const QString &filePath)
{
    m_filePath = filePath;
    load();
    refreshList();
}

void CommentsWidget::setCurrentPage(int page)
{
    m_currentPage = page < 1 ? 1 : page;
}

QString CommentsWidget::storagePath() const
{
    if (m_filePath.isEmpty()) return QString();
    return m_filePath + ".cmt";
}

void CommentsWidget::load()
{
    m_comments.clear();
    const QString path = storagePath();
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto arr = doc.array();
    for (const auto &v : arr) {
        const auto o = v.toObject();
        CommentItem c;
        c.author = o.value("author").toString();
        c.content = o.value("content").toString();
        c.page = o.value("page").toInt(-1);
        c.createdAt = QDateTime::fromString(o.value("createdAt").toString(), Qt::ISODate);
        if (!c.createdAt.isValid()) c.createdAt = QDateTime::currentDateTime();
        if (!c.content.trimmed().isEmpty()) m_comments.append(c);
    }
}

void CommentsWidget::save() const
{
    const QString path = storagePath();
    if (path.isEmpty()) return;

    QJsonArray arr;
    for (const auto &c : m_comments) {
        QJsonObject o;
        o["author"] = c.author;
        o["content"] = c.content;
        o["page"] = c.page;
        o["createdAt"] = c.createdAt.toString(Qt::ISODate);
        arr.append(o);
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void CommentsWidget::refreshList()
{
    m_list->clear();

    if (m_filePath.isEmpty()) {
        m_list->addItem(tr("Open a document to view comments."));
        m_list->setEnabled(false);
        m_author->setEnabled(false);
        m_editor->setEnabled(false);
        m_addBtn->setEnabled(false);
        return;
    }

    m_list->setEnabled(true);
    m_author->setEnabled(true);
    m_editor->setEnabled(true);
    m_addBtn->setEnabled(true);

    if (m_comments.isEmpty()) {
        m_list->addItem(tr("No comments yet."));
        return;
    }

    for (const auto &c : m_comments) {
        const QString pagePart = c.page > 0 ? tr(" • p.%1").arg(c.page) : QString();
        const QString meta = QString("%1 • %2%3")
                                 .arg(c.author.isEmpty() ? tr("Unknown") : c.author)
                                 .arg(c.createdAt.toString("yyyy-MM-dd HH:mm"))
                                 .arg(pagePart);
        const QString body = c.content.trimmed();
        auto *item = new QListWidgetItem(QString("%1\n%2").arg(meta, body), m_list);
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
    }
}

void CommentsWidget::addComment()
{
    if (m_filePath.isEmpty()) return;

    const QString content = m_editor->toPlainText().trimmed();
    if (content.isEmpty()) return;

    CommentItem c;
    c.author = m_author->text().trimmed();
    c.content = content;
    c.createdAt = QDateTime::currentDateTime();
    c.page = m_currentPage;

    m_comments.append(c);
    save();
    m_editor->clear();
    refreshList();
}

